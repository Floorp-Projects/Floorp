#!/bin/bash

# A script to run compile tests with the same condition of the checks done by CI.
#
# Usage
#
# ```sh
# . ./compiletest.sh
# ```

TRYBUILD=overwrite RUSTFLAGS='--cfg pin_project_show_unpin_struct' cargo +nightly test -p pin-project --all-features --test compiletest -- --ignored
# RUSTFLAGS='--cfg pin_project_show_unpin_struct' cargo +nightly test -p pin-project --all-features --test compiletest -- --ignored
