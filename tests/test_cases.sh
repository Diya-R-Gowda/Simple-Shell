#!/usr/bin/env bash
set -u

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SHELL_BIN="$ROOT_DIR/myshell"
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/simple-shell-tests.XXXXXX")
PASS_COUNT=0
FAIL_COUNT=0

cleanup()
{
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

run_case()
{
    local name=$1
    local input=$2
    local expected_status=$3
    local expected_text=$4
    local actual_status

    printf "%s" "$input" | "$SHELL_BIN" >"$TMP_DIR/stdout" 2>"$TMP_DIR/stderr"
    actual_status=$?
    if [ "$actual_status" -eq "$expected_status" ] &&
       grep -Fq "$expected_text" "$TMP_DIR/stdout" "$TMP_DIR/stderr"; then
        printf 'PASS: %s\n' "$name"
        PASS_COUNT=$((PASS_COUNT + 1))
    else
        printf 'FAIL: %s (status %s, expected %s, text %s)\n' \
            "$name" "$actual_status" "$expected_status" "$expected_text"
        cat "$TMP_DIR/stdout" "$TMP_DIR/stderr" >&2
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
}

run_case "external command and PATH lookup" \
    'ls
exit
' 0 "Makefile"
run_case "missing command returns 127" \
    'definitely-not-a-command
exit
' 0 "command not found"
run_case "command containing a slash" \
    '/bin/echo slash
exit
' 0 "slash"
run_case "pwd and echo" \
    'pwd
echo hello world
exit
' 0 "hello world"
run_case "cd valid" \
    "cd /
pwd
exit
" 0 "/"
run_case "cd invalid" \
    'cd /path/that/does/not/exist
exit
' 0 "cd:"
run_case "cd too many arguments" \
    'cd / /tmp
exit
' 0 "too many arguments"
run_case "environment expansion and unset" \
    'export TEST_VALUE=hello
echo $TEST_VALUE ${TEST_VALUE}
unset TEST_VALUE
echo $TEST_VALUE
exit
' 0 "hello hello"
run_case "help" \
    'help
exit
' 0 "Built-ins:"
run_case "exit status" \
    'exit 7
' 7 ""
run_case "output redirection" \
    "echo hi > $TMP_DIR/out
cat $TMP_DIR/out
exit
" 0 "hi"
run_case "append redirection" \
    "echo one > $TMP_DIR/append
echo two >> $TMP_DIR/append
cat $TMP_DIR/append
exit
" 0 "one"
run_case "input redirection" \
    "cat < $TMP_DIR/input
exit
" 0 "input"
printf 'input\n' >"$TMP_DIR/input"
run_case "combined append and input redirection" \
    "cat >> $TMP_DIR/append2 < $TMP_DIR/input
cat $TMP_DIR/append2
exit
" 0 "input"
run_case "missing input file" \
    "cat < $TMP_DIR/missing
exit
" 0 "$TMP_DIR/missing"
run_case "unwritable output path" \
    "echo hi > $TMP_DIR/no-such-dir/out
exit
" 0 "$TMP_DIR/no-such-dir/out"
run_case "two-stage pipe" \
    'printf "a\nb\n" | grep b
exit
' 0 "b"
run_case "three-stage pipe" \
    'printf "a\nb\n" | grep b | tr b B
exit
' 0 "B"
run_case "pipeline builtin output" \
    'echo hi | cat
exit
' 0 "hi"
run_case "pipeline with redirection" \
    "cat < $TMP_DIR/input | tr a-z A-Z > $TMP_DIR/piped
cat $TMP_DIR/piped
exit
" 0 "INPUT"
run_case "empty pipeline side" \
    'echo hi |
exit
' 0 "pipe requires commands"
run_case "background job lifecycle" \
    'sleep 0.1 &
sleep 1
exit
' 0 "Done sleep 0.1"
run_case "background pipeline is one job" \
    'printf "x\n" | cat &
sleep 1
exit
' 0 "Done printf x | cat"
run_case "quoted operators" \
    "echo 'a | b' > $TMP_DIR/quoted
cat $TMP_DIR/quoted
exit
" 0 "a | b"
run_case "unmatched quote" \
    'echo "unterminated
exit
' 0 "unmatched"
run_case "trailing backslash" \
    'echo trailing\
exit
' 0 "trailing escape"
run_case "duplicate redirection" \
    "echo hi > $TMP_DIR/a > $TMP_DIR/b
exit
" 0 "duplicate redirection"
run_case "background operator in middle" \
    'echo hi & echo there
exit
' 0 "must be at the end"
run_case "empty and whitespace lines" \
    '   

echo still-working
exit
' 0 "still-working"

LONG_ARGUMENT=$(printf 'x%.0s' {1..10000})
run_case "very long line" "echo $LONG_ARGUMENT
exit
" 0 "$LONG_ARGUMENT"

if [ "$FAIL_COUNT" -ne 0 ]; then
    printf '%s passed, %s failed\n' "$PASS_COUNT" "$FAIL_COUNT" >&2
    exit 1
fi
printf '%s tests passed\n' "$PASS_COUNT"
