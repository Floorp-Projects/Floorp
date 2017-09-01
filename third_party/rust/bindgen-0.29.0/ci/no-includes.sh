#!/usr/bin/env bash

# Don't allow any system include directives in tests.

set -eu
cd "$(dirname "$0")/.."

echo "Checking for #include directives of system headers..."

grep -rn '#include\s*<.*>' tests/headers || {
    echo "Found none; OK!"
    exit 0
}

echo "
Found a test with an #include directive of a system header file!

There is no guarantee that the system running the tests has the header
file, let alone the same version of it that you have. Any test with such an
include directive won't reliably produce the consistent bindings across systems.
"

exit 1
