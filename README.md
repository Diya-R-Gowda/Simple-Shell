# Simple Shell (Command Line Interpreter)

A simple Unix-like shell written in C that reads commands from the command line and executes them, mimicking the core behavior of shells like `bash` or `dash`.

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

## Project Structure

```
simple-shell/
├── src/
│   ├── main.c          # Entry point, REPL loop
│   ├── parser.c        # Tokenizer / command parsing
│   ├── parser.h
│   ├── executor.c      # fork/exec/wait command execution
│   ├── executor.h
│   ├── builtins.c      # Built-in command implementations
│   ├── builtins.h
│   ├── redirection.c   # I/O redirection and piping logic
│   └── redirection.h
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
