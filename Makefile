# make's built-in default is `cc`, which MinGW does not ship.
ifeq ($(origin CC),default)
CC := gcc
endif
EMCC ?= emcc

ifeq ($(OS),Windows_NT)
EXE := .exe
else
EXE :=
endif

CSTD := -std=c11
WARN := -Wall -Wextra -Wpedantic -Werror
OPT ?= -O2
CFLAGS ?= $(CSTD) $(WARN) $(OPT)
LDLIBS := -lm

BUILD := build
CLI := archmem$(EXE)
CORE_ALL := $(wildcard core/*.c)
# The oracle is a deliberately naive second implementation used only by tests.
CORE := $(filter-out core/oracle.c,$(CORE_ALL))
HEADERS := $(wildcard core/*.h)
TESTS := $(patsubst tests/%.c,$(BUILD)/%$(EXE),$(wildcard tests/test_*.c))
SAN_TESTS := $(patsubst tests/%.c,$(BUILD)/san/%$(EXE),$(wildcard tests/test_*.c))

WASM_OUT := web/public/wasm/archmem.js
WASM_FLAGS := -O2 -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createArchMEM \
	-sENVIRONMENT=web,node -sALLOW_MEMORY_GROWTH=1 \
	-sEXPORTED_FUNCTIONS=_archmem_run,_archmem_free,_malloc,_free \
	-sEXPORTED_RUNTIME_METHODS=UTF8ToString,stringToNewUTF8

.PHONY: all test sanitize cli-test wasm clean help

all: $(CLI)

$(CLI): cli/main.c $(CORE) $(HEADERS)
	$(CC) $(CFLAGS) cli/main.c $(CORE) -o $@ $(LDLIBS)

$(BUILD) $(BUILD)/san:
	mkdir -p $@

$(BUILD)/%$(EXE): tests/%.c $(CORE_ALL) $(HEADERS) tests/test.h | $(BUILD)
	$(CC) $(CFLAGS) $< $(CORE_ALL) -o $@ $(LDLIBS)

test: $(TESTS)
	@for t in $(TESTS); do printf '%-32s ' "$$t"; ./$$t || exit 1; done

# ASan/UBSan are not available with MinGW; on Windows this target says so.
$(BUILD)/san/%$(EXE): tests/%.c $(CORE_ALL) $(HEADERS) tests/test.h | $(BUILD)/san
	$(CC) $(CSTD) $(WARN) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer $< $(CORE_ALL) -o $@ $(LDLIBS)

ifeq ($(OS),Windows_NT)
sanitize:
	@echo "sanitize: ASan/UBSan nao existem no MinGW; rode no Linux/macOS (o CI roda)."
else
sanitize: $(SAN_TESTS)
	@for t in $(SAN_TESTS); do printf '%-36s ' "$$t"; ./$$t || exit 1; done
endif

cli-test: $(CLI)
	sh tests/cli_test.sh ./$(CLI)

wasm: $(CORE) $(HEADERS)
	mkdir -p web/public/wasm
	$(EMCC) $(CSTD) $(WARN) $(CORE) $(WASM_FLAGS) -o $(WASM_OUT)

clean:
	rm -rf $(BUILD) $(CLI) web/public/wasm

help:
	@echo "make            CLI archmem"
	@echo "make test       testes do core"
	@echo "make sanitize   testes com ASan/UBSan (Linux/macOS)"
	@echo "make cli-test   testes da CLI"
	@echo "make wasm       motor em WebAssembly para a web (precisa do emcc)"
	@echo "make clean      remove artefatos"
