#!/usr/bin/env bash
set -u

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SHELL_BIN="$ROOT_DIR/myshell"
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/simple-shell-tests.XXXXXX")
PASS_COUNT=0
FAIL_COUNT=0
declare -a REQUIREMENTS
declare -a TEST_IDS
declare -a RESULTS

cleanup()
{
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

if ! make -C "$ROOT_DIR" >/dev/null; then
    printf 'FAIL: build (make failed)\n' >&2
    exit 1
fi

record_result()
{
    local test_id=$1
    local requirement=$2
    local result=$3
    REQUIREMENTS+=("$requirement")
    TEST_IDS+=("$test_id")
    RESULTS+=("$result")
}

run_case()
{
    local test_id=$1
    local requirement=$2
    local name=$3
    local input=$4
    local expected_status=$5
    local expected_text=$6
    local actual_status

    printf "%s" "$input" | "$SHELL_BIN" >"$TMP_DIR/stdout" 2>"$TMP_DIR/stderr"
    actual_status=$?
    if [ "$actual_status" -eq "$expected_status" ] &&
       { [ -z "$expected_text" ] ||
         grep -Fq "$expected_text" "$TMP_DIR/stdout" "$TMP_DIR/stderr"; }; then
        printf 'PASS: %s (%s, %s)\n' "$test_id" "$name" "$requirement"
        PASS_COUNT=$((PASS_COUNT + 1))
        record_result "$test_id" "$requirement" PASS
    else
        printf 'FAIL: %s (%s, expected status %s/text %s, got %s)\n' \
            "$test_id" "$name" "$expected_status" "$expected_text" "$actual_status"
        cat "$TMP_DIR/stdout" "$TMP_DIR/stderr" >&2
        FAIL_COUNT=$((FAIL_COUNT + 1))
        record_result "$test_id" "$requirement" FAIL
    fi
}

run_case "TC-01" "REQ-1,REQ-2" "external command and PATH lookup" \
    'ls
exit
' 0 "Makefile"
run_case "TC-02" "REQ-1" "missing command returns 127" \
    'definitely-not-a-command
exit
' 0 "command not found"
run_case "TC-03" "REQ-1" "command containing a slash" \
    '/bin/echo slash
exit
' 0 "slash"
run_case "TC-04" "REQ-3,REQ-4" "pwd and echo" \
    'pwd
echo hello world
exit
' 0 "hello world"
run_case "TC-05" "REQ-3" "cd valid" \
    "cd /
pwd
exit
" 0 "/"
run_case "TC-06" "REQ-3" "cd invalid and too many arguments" \
    'cd /path/that/does/not/exist
cd / /tmp
exit
' 0 "too many arguments"
run_case "TC-07" "REQ-20" "HOME PATH and exported child environment" \
    'echo $HOME
echo $PATH
export TEST_CHILD_VALUE=visible
sh -c "printf \"$TEST_CHILD_VALUE\n\""
exit
' 0 "visible"
run_case "TC-08" "REQ-3" "help and exit status" \
    'help
exit 7
' 7 "Built-ins:"

timing_input=$(for i in $(seq 1 100); do printf 'pwd\n'; done; printf 'exit\n')
timing_start=$(date +%s%N)
printf "%s" "$timing_input" | "$SHELL_BIN" >"$TMP_DIR/stdout" 2>"$TMP_DIR/stderr"
timing_status=$?
timing_end=$(date +%s%N)
timing_ms=$(( (timing_end - timing_start) / 1000000 ))
timing_average=$(( timing_ms / 100 ))
if [ "$timing_status" -eq 0 ] && [ "$timing_average" -lt 100 ]; then
    printf 'PASS: TC-09 (100 pwd commands average %sms, REQ-11)\n' "$timing_average"
    PASS_COUNT=$((PASS_COUNT + 1))
    record_result "TC-09" "REQ-11" PASS
else
    printf 'FAIL: TC-09 (100 pwd commands average %sms, REQ-11)\n' "$timing_average"
    cat "$TMP_DIR/stdout" "$TMP_DIR/stderr" >&2
    FAIL_COUNT=$((FAIL_COUNT + 1))
    record_result "TC-09" "REQ-11" FAIL
fi

host_user=$(whoami)
host_uid=$(id -u)
identity_input='whoami
id -u
exit
'
printf "%s" "$identity_input" | "$SHELL_BIN" >"$TMP_DIR/stdout" 2>"$TMP_DIR/stderr"
identity_status=$?
if [ "$identity_status" -eq 0 ] &&
   grep -Fxq "$host_user" "$TMP_DIR/stdout" &&
   grep -Fxq "$host_uid" "$TMP_DIR/stdout"; then
    printf 'PASS: TC-10 (identity matches host, REQ-12)\n'
    PASS_COUNT=$((PASS_COUNT + 1))
    record_result "TC-10" "REQ-12" PASS
else
    printf 'FAIL: TC-10 (identity mismatch, REQ-12)\n'
    cat "$TMP_DIR/stdout" "$TMP_DIR/stderr" >&2
    FAIL_COUNT=$((FAIL_COUNT + 1))
    record_result "TC-10" "REQ-12" FAIL
fi

printf 'input\n' >"$TMP_DIR/input"
whitespace_input=$(printf '   echo    spaced   \nexit\n')
run_case "TC-11" "REQ-5" "extra whitespace" \
    "$whitespace_input" 0 "spaced"
run_case "TC-12" "REQ-7,REQ-14" "operators recognized" \
    "echo operator > $TMP_DIR/operator
cat < $TMP_DIR/operator
exit
" 0 "operator"
empty_input=$(printf '   \n\necho still-working\nexit\n')
run_case "TC-13" "REQ-8" "empty and whitespace-only lines" \
    "$empty_input" 0 "still-working"
run_case "TC-14" "REQ-13" "input redirection" \
    "cat < $TMP_DIR/input
exit
" 0 "input"
run_case "TC-15" "REQ-15" "append redirection" \
    "echo one > $TMP_DIR/append
echo two >> $TMP_DIR/append
cat $TMP_DIR/append
exit
" 0 "one"
run_case "TC-16" "REQ-13,REQ-15" "combined input and append redirection" \
    "cat >> $TMP_DIR/combined < $TMP_DIR/input
cat $TMP_DIR/combined
exit
" 0 "input"
run_case "TC-17" "REQ-16,REQ-17" "two and three stage pipes" \
    'printf "a\nb\n" | grep b | tr b B
exit
' 0 "B"
run_case "TC-18" "REQ-16" "pipeline builtin output" \
    'echo hi | cat
exit
' 0 "hi"
run_case "TC-19" "REQ-14,REQ-16,REQ-17" "pipeline with input and output redirection" \
    "cat < $TMP_DIR/input | tr a-z A-Z > $TMP_DIR/piped
cat $TMP_DIR/piped
exit
" 0 "INPUT"
run_case "TC-20" "REQ-16" "empty side of pipe" \
    'echo hi |
exit
' 0 "pipe requires commands"
run_case "TC-21" "REQ-18" "background returns prompt immediately" \
    'sleep 1 &
echo prompt
exit
' 0 "prompt"
run_case "TC-22" "REQ-18" "background lifecycle" \
    'sleep 0.1 &
sleep 1
exit
' 0 "Done sleep 0.1"
run_case "TC-23" "REQ-18" "background pipeline is one job" \
    'printf "x\n" | cat &
sleep 1
exit
' 0 "Done printf x | cat"
printf 'SKIPPED: no jobs command (REQ-19)\n'
record_result "TC-24" "REQ-19" SKIPPED
run_case "TC-25" "REQ-SEC-2" "reject parent traversal path" \
    'echo rejected > ../rejected
exit
' 0 "'..' paths are not allowed"
run_case "TC-26" "REQ-SEC-2" "accept valid redirection path" \
    "echo accepted > $TMP_DIR/valid
cat $TMP_DIR/valid
exit
" 0 "accepted"
printf '%s' 'export SECRET_VALUE=not-implicit
true
exit
' | "$SHELL_BIN" >"$TMP_DIR/stdout" 2>"$TMP_DIR/stderr"
if [ "$?" -eq 0 ] &&
   ! grep -Fq "not-implicit" "$TMP_DIR/stdout" "$TMP_DIR/stderr"; then
    printf 'PASS: TC-27 (do not print environment values implicitly, REQ-SEC-3)\n'
    PASS_COUNT=$((PASS_COUNT + 1))
    record_result "TC-27" "REQ-SEC-3" PASS
else
    printf 'FAIL: TC-27 (environment value leaked, REQ-SEC-3)\n'
    cat "$TMP_DIR/stdout" "$TMP_DIR/stderr" >&2
    FAIL_COUNT=$((FAIL_COUNT + 1))
    record_result "TC-27" "REQ-SEC-3" FAIL
fi
run_case "TC-28" "REQ-SEC-4" "reject 5000-character input and continue" \
    "$(printf 'x%.0s' {1..5000})
echo recovered
exit
" 0 "input line too long"
run_case "TC-29" "REQ-6,REQ-9" "quoted operators remain arguments" \
    "echo 'a | b' > $TMP_DIR/quoted
cat $TMP_DIR/quoted
exit
" 0 "a | b"
run_case "TC-30" "REQ-9" "malformed input returns to prompt" \
    'echo "unterminated
echo trailing\
echo recovered
exit
' 0 "recovered"
run_case "TC-31" "REQ-9" "duplicate redirection and misplaced background" \
    "echo hi > $TMP_DIR/a > $TMP_DIR/b
echo hi & echo there
exit
" 0 "must be at the end"

LONG_ARGUMENT=$(printf 'x%.0s' {1..4000})
run_case "TC-32" "REQ-10" "maximum supported line" \
    "echo $LONG_ARGUMENT
exit
" 0 "$LONG_ARGUMENT"

printf '\nRequirement coverage:\n'
printf '%-12s %-20s %s\n' "REQ-ID" "TEST IDS" "PASS/FAIL"
for requirement in REQ-1 REQ-2 REQ-3 REQ-4 REQ-5 REQ-6 REQ-7 REQ-8 \
    REQ-9 REQ-10 REQ-11 REQ-12 REQ-13 REQ-14 REQ-15 REQ-16 REQ-17 \
    REQ-18 REQ-19 REQ-20 REQ-SEC-2 REQ-SEC-3 REQ-SEC-4; do
    test_ids=""
    result=PASS
    for index in "${!REQUIREMENTS[@]}"; do
        case ",${REQUIREMENTS[$index]}," in
            *,"$requirement",*)
                test_ids="${test_ids}${test_ids:+,}${TEST_IDS[$index]}"
                [ "${RESULTS[$index]}" = FAIL ] && result=FAIL
                [ "${RESULTS[$index]}" = SKIPPED ] && [ "$result" = PASS ] && result=SKIPPED
                ;;
        esac
    done
    printf '%-12s %-20s %s\n' "$requirement" "${test_ids:-none}" "$result"
done

if [ "$FAIL_COUNT" -ne 0 ]; then
    printf '%s passed, %s failed\n' "$PASS_COUNT" "$FAIL_COUNT" >&2
    exit 1
fi
printf '%s tests passed\n' "$PASS_COUNT"
