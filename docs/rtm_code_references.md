# Requirements Traceability Map

This map reflects the current implementation. A requirement marked
**Partial** or **Not implemented** is intentionally identified rather than
being represented as complete.

| REQ-ID | Requirement summary | Source files and functions | Status |
| --- | --- | --- | --- |
| REQ-1 | Display prompt and read input promptly | `src/main.c`: `main` | Implemented |
| REQ-2 | Continue until `exit` or EOF | `src/main.c`: `main` | Implemented |
| REQ-3 | Support core built-ins | `src/builtins.c`: `run_builtin`; `src/executor.c`: `execute_commands` | Implemented |
| REQ-4 | Report invalid `cd` without changing directory | `src/builtins.c`: `run_builtin` | Implemented |
| REQ-5 | Handle consecutive whitespace | `src/parser.c`: `parse_line`, `parse_word` | Implemented |
| REQ-6 | Treat quoted text as one argument | `src/parser.c`: `parse_word` | Implemented |
| REQ-7 | Recognize shell operators | `src/parser.c`: `parse_line` | Implemented |
| REQ-8 | Accept empty and whitespace-only lines | `src/main.c`: `main`; `src/parser.c`: `parse_line` | Implemented |
| REQ-9 | Execute external commands with fork/exec | `src/executor.c`: `execute_command`, `exec_command` | Implemented |
| REQ-10 | Resolve bare names through PATH | `src/environment.c`: `resolve_executable_path`; `src/executor.c`: `exec_command` | Implemented |
| REQ-11 | Wait for foreground children | `src/executor.c`: `execute_command`, `execute_pipeline` | Implemented |
| REQ-12 | Clear error and status for missing commands | `src/executor.c`: `exec_command` | Implemented |
| REQ-13 | Redirect stdin with `<` | `src/redirection.c`: `apply_redirection` | Implemented |
| REQ-14 | Truncate stdout with `>` | `src/redirection.c`: `apply_redirection` | Implemented |
| REQ-15 | Append stdout with `>>` | `src/redirection.c`: `apply_redirection` | Implemented |
| REQ-16 | Chain commands with pipes | `src/executor.c`: `execute_pipeline` | Implemented |
| REQ-17 | Close unused pipe descriptors | `src/executor.c`: `execute_pipeline`, `close_pipes` | Implemented |
| REQ-18 | Run commands in the background | `src/executor.c`: `run_in_background`, `execute_pipeline` | Implemented |
| REQ-19 | View background jobs on request | `src/executor.c`: `JobList`, `reap_finished_jobs` | Partial: tracking exists; no `jobs` built-in |
| REQ-20 | Read and pass environment variables | `src/environment.c`: `shell_getenv`; `src/builtins.c`: `run_builtin`; `src/parser.c`: `expand_variable` | Implemented |
| REQ-21 | Maintain command history | No implementation | Not implemented |
| REQ-22 | Handle SIGINT at the prompt | No implementation | Not implemented |
| REQ-23 | Handle SIGTSTP for foreground jobs | No implementation | Not implemented |
| REQ-SEC-1 | Do not escalate privileges | `src/executor.c`: fork/exec paths; no setuid/setgid calls | Implemented |
| REQ-SEC-2 | Validate and canonicalize redirection paths | `src/redirection.c`: `has_parent_component`, `canonical_redirection_input`, `canonical_redirection_output`, `apply_redirection` | Implemented |
| REQ-SEC-3 | Do not expose environment values implicitly | `src/environment.c`: `shell_getenv`; `src/parser.c`: `expand_variable` | Implemented |
| REQ-SEC-4 | Enforce input and argument limits | `src/limits.h`; `src/main.c`: bounded input; `src/parser.c`: argument-count check | Implemented |
