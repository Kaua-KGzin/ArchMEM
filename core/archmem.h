/* ArchMEM public API.
 *
 * The whole engine is reached through one JSON boundary so the native CLI
 * and the WebAssembly build behave identically. See the JSON contract in
 * docs/superpowers/plans/2026-11-24-archmem.md and docs/api.md. */
#ifndef ARCHMEM_H
#define ARCHMEM_H

#define ARCHMEM_VERSION "2.0.0"

/* Runs one request (a JSON object with an "action") and returns a newly
 * allocated JSON response. Never returns NULL: failures are returned as
 * {"ok":false,"error":"..."}. Free the result with archmem_free. */
char *archmem_run(const char *request_json);
void archmem_free(char *response);

#endif
