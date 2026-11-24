#!/bin/sh
# CLI tests: sh tests/cli_test.sh ./archmem
set -u
BIN=${1:-./archmem}
S="7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1"
fails=0
checks=0

expect() { # expect <description> <condition result 0/1>
    checks=$((checks + 1))
    if [ "$2" -ne 0 ]; then
        fails=$((fails + 1))
        echo "  FAIL: $1"
    fi
}

out=$("$BIN" --version); expect "--version prints 2.0.0" "$([ "$out" = "2.0.0" ]; echo $?)"

out=$("$BIN" compare --frames 3 --refs "$S" --algos fifo,lru,opt)
for n in 15 12 9; do
    expect "compare shows $n" "$(echo "$out" | grep -q "$n"; echo $?)"
done
expect "compare ranks opt first" "$(echo "$out" | grep -A1 -iE 'algoritmo' | tail -1 | grep -qi opt; echo $?)"

out=$("$BIN" run --algo lru --frames 3 --refs "$S" --json)
expect "run --json is the engine response" "$(echo "$out" | grep -q '"ok":true'; echo $?)"
expect "run --json has 12 faults" "$(echo "$out" | grep -q '"faults":12'; echo $?)"

out=$("$BIN" run --algo lru --frames 3 --refs "$S" --steps)
expect "run --steps has a row per access" "$(echo "$out" | grep -cE '^ *[0-9]+ \|' | grep -qx 20; echo $?)"
expect "run shows hit rate" "$(echo "$out" | grep -q '40'; echo $?)"

out=$("$BIN" curve --algos fifo --max-frames 5 --refs "1 2 3 4 1 2 5 1 2 3 4 5")
expect "curve reports Belady" "$(echo "$out" | grep -qi 'belady'; echo $?)"

out=$("$BIN" gen --kind loop --length 6 --pages 3)
expect "gen loop" "$([ "$out" = "0 1 2 0 1 2" ]; echo $?)"

out=$("$BIN" seg --fit best --memory 1000 --ops "alloc A 300; alloc B 200; free A; alloc C 100")
expect "seg shows fragmentation" "$(echo "$out" | grep -qi 'fragmenta'; echo $?)"

out=$("$BIN" challenge --seed 3 --difficulty 1 --answer)
expect "challenge prints an answer" "$(echo "$out" | grep -qi 'resposta'; echo $?)"

"$BIN" run --frames 0 --refs 1 >/dev/null 2>&1; rc=$?
expect "engine error exits 1" "$([ $rc -eq 1 ]; echo $?)"
"$BIN" bogus >/dev/null 2>&1; rc=$?
expect "unknown command exits 2" "$([ $rc -eq 2 ]; echo $?)"
"$BIN" run --frames >/dev/null 2>&1; rc=$?
expect "flag without value exits 2" "$([ $rc -eq 2 ]; echo $?)"

tmp=${TMPDIR:-/tmp}/archmem-scenario-$$.json
printf '{"action":"run","config":{"frames":3,"algorithm":"opt"},"refs":"%s"}' "$S" > "$tmp"
out=$("$BIN" run --scenario "$tmp" --json)
expect "run --scenario" "$(echo "$out" | grep -q '"faults":9'; echo $?)"
out=$("$BIN" --raw "$tmp")
expect "--raw prints engine output" "$(echo "$out" | grep -q '"faults":9'; echo $?)"
rm -f "$tmp"

if [ "$fails" -ne 0 ]; then
    echo "cli: $fails of $checks checks FAILED"
    exit 1
fi
echo "cli: ok $checks checks"
