# The option `Z` is only accepted on the nightly compiler
# so changing to nightly toolchain by `rustup default nightly` is required.

# See: https://github.com/rust-lang/rust/issues/39699 for more sanitizer support.

toolchain=$(rustup default)
echo "\nUse Rust toolchain: $toolchain"

if [[ $toolchain != nightly* ]]; then
    echo "The sanitizer is only available on Rust Nightly only. Skip."
    exit
fi

sanitizers=("address" "leak" "memory" "thread")
for san in "${sanitizers[@]}"
do
    San="$(tr '[:lower:]' '[:upper:]' <<< ${san:0:1})${san:1}"
    echo "\n\nRun ${San}Sanitizer\n------------------------------"
    RUSTFLAGS="-Z sanitizer=${san}" sh run_tests.sh
done
