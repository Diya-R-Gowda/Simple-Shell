# Simple Shell (Command Line Interpreter)

A simple Unix-like shell written in C that reads commands from the command line and executes them, mimicking the core behavior of shells like `bash` or `dash`.

## Project Description

This project is based on one of the assigned sample software projects — **Simple Shell (Command Line Interpreter)** — rather than being a brand-new, team-proposed idea. The goal is to build a simple Unix-like shell in C that reads commands typed by a user, interprets them, and executes them, replicating the core behavior of standard shells such as `bash` or `dash`.

There is currently no external customer or interested party for this project; it is being developed purely as a coursework deliverable, with the course instructors/evaluators as the primary stakeholders.

The typical users of this system are people who are comfortable working in a command-line environment and want a functioning, minimal shell to run everyday commands — navigating directories, running programs, redirecting input/output, and chaining commands together with pipes. It would also be of interest to anyone wanting to understand, at a hands-on level, how a shell works internally (process creation via `fork`/`exec`, parsing, and I/O handling), since the project exposes all of that logic directly rather than hiding it behind an existing shell.

By the end of this project, the user will be able to:

- Launch the shell and interact with it through a continuous prompt (REPL)
- Run any standard external command available on the system (e.g., `ls`, `cat`, `grep`)
- Use built-in commands such as `cd`, `exit`, `pwd`, and `echo`
- Redirect a command's input or output to/from a file using `<`, `>`, and `>>`
- Chain multiple commands together using pipes (`|`)
- Run commands in the background using `&`
- Receive clear error messages for invalid commands, missing files, or malformed input, without the shell crashing

## Purpose

This project implements a lightweight command-line interpreter (shell) that allows a user to enter commands and have them executed by the operating system, just like a standard Unix shell. It covers the fundamental mechanics of process creation, command parsing, and I/O handling that underlie all shell programs.

## Scope

The system is a standalone, self-contained command-line application. It does not depend on or extend any existing shell implementation — it is built from scratch in C using standard POSIX system calls. The scope covers reading input, interpreting it, executing commands (built-in or external), and returning output to the user in a continuous loop.

## Intended Users

- Users comfortable with a command-line interface who want to run standard shell commands (navigation, file operations, piping, redirection).
- Anyone evaluating or studying how a basic shell is implemented internally (process creation, parsing, I/O redirection).

## Functional Features

- **Read-Eval-Print Loop (REPL)** — Continuously displays a prompt, reads a command line from the user, executes it, and loops until the user exits.
- **Command Parsing / Tokenization** — Splits raw input into a command and its arguments, correctly handling extra whitespace, quoted strings, and special characters.
- **Command Execution** — Runs external programs using `fork()`, `exec()`, and `wait()`, with proper exit-status reporting back to the shell.
- **Built-in Commands** — Supports commands that must run in the shell's own process rather than a child process, such as `cd`, `exit`, `pwd`, `echo`, and `help`.
- **I/O Redirection** — Supports redirecting input and output using `<`, `>`, and `>>`.
- **Piping** — Supports chaining multiple commands together using `|`, passing the output of one command as input to the next.
- **Background Execution** — Supports running commands in the background using `&`, along with basic job tracking.
- **Environment Variable & PATH Resolution** — Resolves executables using the system `PATH` and supports basic environment variable handling.
- **Error Handling** — Gracefully handles invalid commands, missing files, permission errors, and malformed input without crashing the shell.
- **(Stretch Goal) Command History** — Recalls and re-executes previously entered commands.
- **(Stretch Goal) Signal Handling** — Properly handles signals such as `Ctrl+C` (SIGINT) and `Ctrl+Z` (SIGTSTP).

## Non-Functional Requirements

- **Performance** — Built-in commands should return control to the prompt with negligible delay (target: under 100ms for built-ins under normal load).
- **Reliability** — The shell must not crash or hang on invalid input, missing files, or failed system calls; it should print a clear error and return to the prompt.
- **Portability** — The shell should compile and run on any POSIX-compliant system (Linux/macOS) without modification.
- **Usability** — Error messages should be clear enough for a user to understand what went wrong and how to correct it.
- **Maintainability** — Code should be modular (separate parsing, execution, redirection, built-ins) so features can be extended independently.

## Design Constraints

- Implemented in **C** (per project specification), using standard POSIX system calls (`fork`, `exec`, `pipe`, `dup2`, `wait`, etc.).
- No reliance on third-party shell libraries — core logic must be implemented directly.
- Must run in a standard terminal environment; no GUI component.

## Task Assignment (Current Sprint)

| Owner | Functional Feature(s) | Qualitative Property |
| --- | --- | --- |
| **Prithviraj** | REQ-1–REQ-4: REPL loop (prompt/read/execute cycle) and built-ins: `cd`, `exit`, `pwd`, `echo`, `help` | Near-instant response for built-ins; clear error message on invalid usage |
| **Aarush** | REQ-5–REQ-8: parsing/tokenization; REQ-18–REQ-19: background execution and job IDs | Robust malformed-input handling; prompt returns without waiting for background work |
| **Debhargo** | REQ-9–REQ-12: command execution; REQ-16–REQ-17: piping and descriptor cleanup | Correct fork/exec/wait behavior, PATH-independent execution, and no pipeline hangs |
| **Diya** | REQ-13–REQ-15: redirection; REQ-20: environment/PATH handling; REQ-SEC-2 security validation | Correct combined redirection and environment behavior with validated file paths |

> Everyone is expected to test and review across all modules, not just their own — per team guidelines.

## Project Structure

```
simple-shell/
├── src/
│   ├── main.c          # Entry point, REPL loop
│   ├── parser.c        # Tokenizer / command parsing
│   ├── parser.h
│   ├── executor.h
│   ├── builtins.c      # Built-in command implementations
│   ├── builtins.h
│   ├── environment.c   # Environment access and PATH resolution
│   ├── environment.h
│   └── executor.c      # Redirection, pipelines, jobs, and fork/exec/wait
├── tests/
│   └── test_cases.sh   # Shell script with sample test commands
├── Makefile
└── README.md
```

## Build Instructions

```bash
git clone <repo-url>
cd simple-shell
make
```

This will compile the project and produce an executable named `myshell` (or as defined in the Makefile).

## Implementation Notes

- Redirection is applied in children before `execv()`. In-process built-ins
  temporarily save standard input and output, apply redirection, then restore
  the saved descriptors after flushing output.
- Pipelines create all pipes before forking. Each child connects only its
  adjacent pipe ends, and both parent and children close every unused pipe end
  to prevent descriptor leaks and pipeline hangs.
- Background jobs store all process IDs for a pipeline as one job. Finished
  processes are checked with nonblocking `waitpid(..., WNOHANG)` at the start
  of each prompt iteration.
- `resolve_executable_path()` uses a command containing `/` unchanged.
  Otherwise it searches each `PATH` entry with `access(..., X_OK)` and
  requires the result to be a regular file before `execv()` is called.
- The parser validates operators and expands `$NAME` and `${NAME}` from the
  shell environment. `export NAME[=VALUE] [...]` and `unset NAME` update the
  environment used by later commands.
- Input is bounded to 4096 characters per line and 256 arguments per command.
- Redirection rejects empty paths and `..` path components. Existing input paths
  and output parent directories are canonicalized with `realpath()` before
  `open()` is called.

## Testing

Build and run the regression suite with:

```bash
make test
```

The suite in `tests/test_cases.sh` runs the shell non-interactively and covers
PATH lookup, built-ins, redirection, pipelines, background jobs, parser errors,
and malformed or unusually long input.

## Known Limitations

- `2>` and `&&` are not supported.
- `&` is supported only at the end of a command line.
- There is no `fg` or `bg` job-control command.
- Command history and interactive signal handling are not implemented.

## Usage

```bash
./myshell
```

Once launched, you'll see a prompt where you can type standard shell commands:

```
myshell> ls -l
myshell> cd /home/user
myshell> echo "Hello World" > output.txt
myshell> cat output.txt | grep Hello
myshell> sleep 10 &
myshell> exit
```

## Requirements

- GCC or any C99-compatible compiler
- POSIX-compliant OS (Linux/macOS) — relies on `fork()`, `exec()`, `pipe()`, `dup2()`, and related system calls
- `make`

## License

This project is developed as part of a coursework submission and is not currently licensed for external use.