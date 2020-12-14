#!/bin/sh

# test.sh - Run some tests.

set -eu

# announce what you're doing before you do it
verbosely() {
    echo "$*"
    $*
}

wtf() {
    exitcode="$?"
    if [ $(which python3 | cut -b -4) == "/usr" ]; then
        echo >&2
        echo "WARNING: venv is not activated. See README.md." >&2
    fi
    exit $exitcode
}

warn_update() {
    exitcode="$?"
    echo >&2
    echo "NOTE: Test failed. This may just mean you need to run update.sh." >&2
    exit $exitcode
}

verbosely python3 -m tests.test || wtf
verbosely python3 -m tests.test_js
verbosely python3 -m tests.test_parse_pgen || warn_update
