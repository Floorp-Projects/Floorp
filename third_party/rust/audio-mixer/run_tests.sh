# Bail out once getting an error.
set -e

# Display verbose backtrace for debugging if backtrace is unset
if [ -z "${RUST_BACKTRACE}" ]; then
  export RUST_BACKTRACE=1
fi
echo -e "RUST_BACKTRACE is set to ${RUST_BACKTRACE}\n"

# Format check
cargo fmt --all -- --check

# Lints check
cargo clippy --all-targets --all-features -- -D warnings

# Regular Tests
cargo test --verbose --all
