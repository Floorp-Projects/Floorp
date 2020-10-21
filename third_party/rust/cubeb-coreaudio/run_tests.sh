# Bail out once getting an error.
set -e

echo "\n\nTest suite for cubeb-coreaudio\n========================================"

if [[ -z "${RUST_BACKTRACE}" ]]; then
    # Display backtrace for debugging
    export RUST_BACKTRACE=1
fi
echo "RUST_BACKTRACE is set to ${RUST_BACKTRACE}\n"

# Run tests in the sub crate
# Run the tests by `cargo * -p <SUB_CRATE>` if it's possible. By doing so, the duplicate compiling
# between this crate and the <SUB_CRATE> can be saved. The compiling for <SUB_CRATE> can be reused
# when running `cargo *` with this crate.
# -------------------------------------------------------------------------------------------------
SUB_CRATE="coreaudio-sys-utils"

# Format check
# `cargo fmt -p *` is only usable in workspaces, so a workaround is to enter to the sub crate
# and then exit from it.
cd $SUB_CRATE
cargo fmt --all -- --check
cd ..

# Lints check
cargo clippy -p $SUB_CRATE -- -D warnings

# Regular Tests
cargo test -p $SUB_CRATE

# Run tests in the main crate
# -------------------------------------------------------------------------------------------------
# Format check
cargo fmt --all -- --check

# Lints check
cargo clippy -- -D warnings

# Regular Tests
cargo test --verbose
cargo test test_configure_output -- --ignored
cargo test test_aggregate -- --ignored --test-threads=1

# Parallel Tests
cargo test test_parallel -- --ignored --nocapture --test-threads=1

# Device-changed Tests
sh run_device_tests.sh

# Manual Tests
# cargo test test_switch_output_device -- --ignored --nocapture
# cargo test test_device_collection_change -- --ignored --nocapture
# cargo test test_stream_tester -- --ignored --nocapture
