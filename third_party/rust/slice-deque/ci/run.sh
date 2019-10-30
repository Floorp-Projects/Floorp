#!/usr/bin/env bash

set -ex

export RUST_TEST_THREADS=1
export RUST_BACKTRACE=1
export RUST_TEST_NOCAPTURE=1
export OPT="--target=$TARGET"
export OPT_RELEASE="--release ${OPT}"
export OPT_ND="--no-default-features ${OPT}"
export OPT_RELEASE_ND="--no-default-features ${OPT_RELEASE}"

# Select cargo command: use cross by default
export CARGO_CMD=cross

# On Appveyor (windows) and Travis (x86_64-unknown-linux-gnu and apple) native targets we use cargo (no need to cross-compile):
if [[ $TARGET = *"windows"* ]] || [[ $TARGET == "x86_64-unknown-linux-gnu" ]] || [[ $TARGET = *"apple"* ]]; then
    export CARGO_CMD=cargo
fi

# Install cross if necessary:
if [[ $CARGO_CMD == "cross" ]]; then
    cargo install cross
fi

# Use iOS simulator for those targets that support it:
if [[ $TARGET = *"ios"* ]]; then
    export RUSTFLAGS=-Clink-arg=-mios-simulator-version-min=7.0
    rustc ./ci/deploy_and_run_on_ios_simulator.rs -o $HOME/runtest
    export CARGO_TARGET_X86_64_APPLE_IOS_RUNNER=$HOME/runtest
    export CARGO_TARGET_I386_APPLE_IOS_RUNNER=$HOME/runtest
fi

# Make sure TARGET is installed when using cargo:
if [[ $CARGO_CMD == "cargo" ]]; then
    rustup target add $TARGET || true
fi

# If the build should not run tests, just check that the code builds:
if [[ $NORUN == "1" ]]; then
    export CARGO_SUBCMD="build"
else
    export CARGO_SUBCMD="test"
    # If the tests should be run, always dump all test output.
    export OPT="${OPT} "
    export OPT_RELEASE="${OPT_RELEASE} "
    export OPT_ND="${OPT_ND} "
    export OPT_RELEASE_ND="${OPT_RELEASE_ND} "
fi

# Run all the test configurations:
if [[ $NOSTD != "1" ]]; then # These builds require a std component
    $CARGO_CMD $CARGO_SUBCMD $OPT
    $CARGO_CMD $CARGO_SUBCMD $OPT_RELEASE
fi

$CARGO_CMD $CARGO_SUBCMD $OPT_ND
! find target/ -name *.rlib -exec nm {} \; | grep "std"
$CARGO_CMD clean
$CARGO_CMD $CARGO_SUBCMD $OPT_RELEASE_ND
! find target/ -name *.rlib -exec nm {} \; | grep "std"

if [[ $NOSTD != "1" ]]; then # These builds require a std component
    $CARGO_CMD $CARGO_SUBCMD --features "use_std" $OPT_ND
    $CARGO_CMD $CARGO_SUBCMD --features "use_std" $OPT_RELEASE_ND

    #$CARGO_CMD $CARGO_SUBCMD --features "bytes_buf" $OPT_ND
    #$CARGO_CMD $CARGO_SUBCMD --features "bytes_buf" $OPT_RELEASE_ND
fi

if [[ $TRAVIS_RUST_VERSION == "nightly" ]]; then
    $CARGO_CMD $CARGO_SUBCMD --features "unstable" $OPT_ND
    $CARGO_CMD $CARGO_SUBCMD --features "unstable" $OPT_RELEASE_ND

    if [[ $NOSTD != "1" ]]; then # These builds require a std component

        $CARGO_CMD $CARGO_SUBCMD --features "use_std,unstable" $OPT_ND
        $CARGO_CMD $CARGO_SUBCMD --features "use_std,unstable" $OPT_RELEASE_ND

        #$CARGO_CMD $CARGO_SUBCMD --features "unstable,bytes_buf" $OPT_ND
        #$CARGO_CMD $CARGO_SUBCMD --features "unstable,bytes_buf" $OPT_RELEASE_ND

        if [[ $SYSV == "1" ]]; then
            $CARGO_CMD $CARGO_SUBCMD --features "use_std,unstable,unix_sysv" $OPT
            $CARGO_CMD $CARGO_SUBCMD --features "use_std,unstable,unix_sysv" $OPT_RELEASE_ND
        fi
    fi
fi

if [[ $SYSV == "1" ]]; then
    $CARGO_CMD $CARGO_SUBCMD --features "unix_sysv" $OPT_
    $CARGO_CMD $CARGO_SUBCMD --features "unix_sysv" $OPT_RELEASE_ND
    if [[ $NOSTD != "1" ]]; then # These builds require a std component
        $CARGO_CMD $CARGO_SUBCMD --features "use_std,unix_sysv" $OPT_ND
        $CARGO_CMD $CARGO_SUBCMD --features "use_std,unix_sysv" $OPT_RELEASE_ND
    fi
fi

# Run documentation and clippy:
if [[ $CARGO_CMD == "cargo" ]] && [[ $TARGET != *"ios"* ]]; then
    cargo doc
    if [[ $TRAVIS_RUST_VERSION == "nightly" ]]; then
        rustup component add clippy-preview
        cargo clippy -- -D clippy-pedantic
    fi
fi
