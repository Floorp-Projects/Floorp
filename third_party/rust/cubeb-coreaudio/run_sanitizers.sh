# The option `Z` is only accepted on the nightly compiler
# so changing to nightly toolchain by `rustup default nightly` is required.

# See: https://github.com/rust-lang/rust/issues/39699 for more sanitizer support.

toolchain=$(rustup default)
echo "\nUse Rust toolchain: $toolchain"

if [[ $toolchain == nightly-* ]]
then
   echo "Run sanitizers!"
else
   echo "The sanitizer is only available on Rust Nightly only. Skip."
   exit
fi

# Run tests in the sub crate
# -------------------------------------------------------------------------------------------------
cd coreaudio-sys-utils

echo "\n\nRun ASan\n-----------\n"
RUSTFLAGS="-Z sanitizer=address" cargo test

echo "\n\nRun LSan\n-----------\n"
RUSTFLAGS="-Z sanitizer=leak" cargo test

echo "\n\nRun MSan\n-----------\n"
RUSTFLAGS="-Z sanitizer=memory" cargo test

echo "\n\nRun TSan\n-----------\n"
RUSTFLAGS="-Z sanitizer=thread" cargo test

cd ..

# Run tests in the main crate
# -------------------------------------------------------------------------------------------------
echo "\n\nRun ASan\n-----------\n"
RUSTFLAGS="-Z sanitizer=address" sh run_tests.sh

echo "\n\nRun LSan\n-----------\n"
RUSTFLAGS="-Z sanitizer=leak" sh run_tests.sh

echo "\n\nRun MSan\n-----------\n"
RUSTFLAGS="-Z sanitizer=memory" sh run_tests.sh

echo "\n\nRun TSan\n-----------\n"
RUSTFLAGS="-Z sanitizer=thread" sh run_tests.sh
