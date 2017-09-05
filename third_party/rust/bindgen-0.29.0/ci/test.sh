#!/usr/bin/env bash

set -xeu
cd "$(dirname "$0")/.."

export RUST_BACKTRACE=1

# Disallow system header file includes in our test suite.
./ci/no-includes.sh

# Regenerate the test headers' bindings in debug and release modes, and assert
# that we always get the expected generated bindings.

cargo test --features "$BINDGEN_FEATURES"
./ci/assert-no-diff.sh

cargo test --features "$BINDGEN_FEATURES testing_only_extra_assertions"
./ci/assert-no-diff.sh

cargo test --release --features "$BINDGEN_FEATURES testing_only_extra_assertions"
./ci/assert-no-diff.sh

if [ -v "${TRAVIS_OS_NAME}" ]; then

    # Now test the expectations' size and alignment tests.

    pushd tests/expectations
    cargo test
    cargo test --release
    popd

    # And finally, test our example bindgen + build.rs integration template project.

    cd bindgen-integration
    cargo test --features "$BINDGEN_FEATURES"
    cargo test --release --features "$BINDGEN_FEATURES"

fi
