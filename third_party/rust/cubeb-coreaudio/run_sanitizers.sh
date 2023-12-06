# The option `Z` is only accepted on the nightly compiler
# so changing to nightly toolchain by `rustup default nightly` is required.

# See: https://github.com/rust-lang/rust/issues/39699 for more sanitizer support.

toolchain=$(rustup default)
echo "\nUse Rust toolchain: $toolchain"

if [[ $toolchain != nightly* ]]; then
    echo "The sanitizer is only available on Rust Nightly only. Skip."
    exit
fi

# Bail out once getting an error.
set -e

# Ideally, sanitizers should be ("address" "leak" "memory" "thread") but
# - `memory`: It doesn't works with target x86_64-apple-darwin
# - `leak`: Get some errors that are out of our control. See:
#   https://github.com/mozilla/cubeb-coreaudio-rs/issues/45#issuecomment-591642931
sanitizers=("address" "thread")
for san in "${sanitizers[@]}"
do
    San="$(tr '[:lower:]' '[:upper:]' <<< ${san:0:1})${san:1}"
    echo "\n\nRun ${San}Sanitizer\n------------------------------"
    RUSTFLAGS="-Z sanitizer=${san}" sh run_tests.sh
done
