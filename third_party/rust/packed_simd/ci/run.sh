#!/usr/bin/env bash

set -ex

: ${TARGET?"The TARGET environment variable must be set."}

# Tests are all super fast anyway, and they fault often enough on travis that
# having only one thread increases debuggability to be worth it.
#export RUST_TEST_THREADS=1
#export RUST_BACKTRACE=full
#export RUST_TEST_NOCAPTURE=1

# Some appveyor builds run out-of-memory; this attempts to mitigate that:
# https://github.com/rust-lang-nursery/packed_simd/issues/39
# export RUSTFLAGS="${RUSTFLAGS} -C codegen-units=1"
# export CARGO_BUILD_JOBS=1

export CARGO_SUBCMD=test
if [[ "${NORUN}" == "1" ]]; then
    export CARGO_SUBCMD=build
fi

if [[ ${TARGET} == "x86_64-apple-ios" ]] || [[ ${TARGET} == "i386-apple-ios" ]]; then
    export RUSTFLAGS="${RUSTFLAGS} -Clink-arg=-mios-simulator-version-min=7.0"
    rustc ./ci/deploy_and_run_on_ios_simulator.rs -o $HOME/runtest
    export CARGO_TARGET_X86_64_APPLE_IOS_RUNNER=$HOME/runtest
    export CARGO_TARGET_I386_APPLE_IOS_RUNNER=$HOME/runtest
fi

# The source directory is read-only. Need to copy internal crates to the target
# directory for their Cargo.lock to be properly written.
mkdir target || true

rustc --version
cargo --version
echo "TARGET=${TARGET}"
echo "HOST=${HOST}"
echo "RUSTFLAGS=${RUSTFLAGS}"
echo "NORUN=${NORUN}"
echo "NOVERIFY=${NOVERIFY}"
echo "CARGO_SUBCMD=${CARGO_SUBCMD}"
echo "CARGO_BUILD_JOBS=${CARGO_BUILD_JOBS}"
echo "CARGO_INCREMENTAL=${CARGO_INCREMENTAL}"
echo "RUST_TEST_THREADS=${RUST_TEST_THREADS}"
echo "RUST_BACKTRACE=${RUST_BACKTRACE}"
echo "RUST_TEST_NOCAPTURE=${RUST_TEST_NOCAPTURE}"

cargo_test() {
    cmd="cargo ${CARGO_SUBCMD} --verbose --target=${TARGET} ${@}"
    if [ "${NORUN}" != "1" ]
    then
        if [ "$TARGET" != "wasm32-unknown-unknown" ]
        then
            cmd="$cmd -- --quiet"
        fi
    fi
    mkdir target || true
    ${cmd} 2>&1 | tee > target/output
    if [[ ${PIPESTATUS[0]} != 0 ]]; then
        cat target/output
        return 1
    fi
}

cargo_test_impl() {
    ORIGINAL_RUSTFLAGS=${RUSTFLAGS}
    RUSTFLAGS="${ORIGINAL_RUSTFLAGS} --cfg test_v16  --cfg test_v32 --cfg test_v64" cargo_test ${@}
    RUSTFLAGS="${ORIGINAL_RUSTFLAGS} --cfg test_v128 --cfg test_v256" cargo_test ${@}
    RUSTFLAGS="${ORIGINAL_RUSTFLAGS} --cfg test_v512" cargo_test ${@}
    RUSTFLAGS=${ORIGINAL_RUSTFLAGS}
}

# Debug run:
if [[ "${TARGET}" != "wasm32-unknown-unknown" ]]; then
   # Run wasm32-unknown-unknown in release mode only
   cargo_test_impl
fi

if [[ "${TARGET}" == "x86_64-unknown-linux-gnu" ]] || [[ "${TARGET}" == "x86_64-pc-windows-msvc" ]]; then
    # use sleef on linux and windows x86_64 builds
    cargo_test_impl --release --features=into_bits,core_arch,sleef-sys
else
    cargo_test_impl --release --features=into_bits,core_arch
fi

# Verify code generation
if [[ "${NOVERIFY}" != "1" ]]; then
    cp -r verify/verify target/verify
    export STDSIMD_ASSERT_INSTR_LIMIT=30
    if [[ "${TARGET}" == "i586-unknown-linux-gnu" ]]; then
        export STDSIMD_ASSERT_INSTR_LIMIT=50
    fi
    cargo_test --release --manifest-path=target/verify/Cargo.toml
fi

. ci/run_examples.sh
