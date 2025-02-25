#!/usr/bin/env bash

usage() {
    cat << EOF
Usage: $(basename "$0") [-h|-d] /path/to/ploy /path/to/test/cases/dir

Options:
    -h      Display help message and quit.
    -d      Skip output checks and show disassembly of all test cases.
EOF
}

disassemble=""

while getopts :hd opt; do
    case $opt in
        h)
            usage
            exit
            ;;
        d)
            disassemble="true"
            ;;
        \?)
            echo "error: invalid option -$OPTARG"
            usage
            exit 1
            ;;
        :)
            echo "error: missing argument for option -$OPTARG"
            usage
            exit 1
            ;;
    esac
done

shift $((OPTIND-1))

if [[ $# -ne 2 ]]; then
    echo "error: missing arguments"
    usage
    exit 1
fi

ploy_exe="$1"
test_cases_dir="$2"

errors_encountered=""

for test_case in "$test_cases_dir"/*.scm; do
    if [[ -n $disassemble ]]; then
        "$ploy_exe" -d "$test_case" || errors_encountered="true"
        continue
    fi

    test_case_output="$("$ploy_exe" "$test_case")"

    if [[ $? -ne 0 ]]; then
        errors_encountered="true"
        echo -e "error: test case $test_case returned non-zero status:\n$test_case_output"
        continue
    fi

    expected_output="$(sed -nr 's/;; (.+)/\1/p' "$test_case")"

    if [[ $test_case_output != $expected_output ]]; then
        errors_encountered="true"
        echo "error: test case $test_case failed:"
        echo -e "expected output:\n$expected_output"
        echo -e "actual output:\n$test_case_output"
    fi
done

if [[ -n $errors_encountered ]]; then
    echo "one or more errors has occured"
    exit 1
fi
