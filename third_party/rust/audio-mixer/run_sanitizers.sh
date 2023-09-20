# The option `Z` is only accepted on the nightly compiler
# so changing to nightly toolchain by `rustup default nightly` is required.

# See: https://github.com/rust-lang/rust/issues/39699 for more sanitizer support.

toolchain="$(rustup default)"
echo -e "\nUse Rust toolchain: $toolchain"

NIGHTLY_PREFIX="nightly"
if [ ! -z "${toolchain##$NIGHTLY_PREFIX*}" ]; then
  echo -e "The sanitizer is only available on Rust Nightly only. Skip."
  exit
fi

if [ -z "${toolchain##*windows*}" ]; then
  echo -e "The sanitizer doesn't work on Windows platform yet"
  exit
fi

# Display verbose backtrace for debugging if backtrace is unset
if [ -z "${RUST_BACKTRACE}" ]; then
  export RUST_BACKTRACE=1
fi
echo -e "RUST_BACKTRACE is set to ${RUST_BACKTRACE}\n"

# {Address, Thread}Sanitizer cannot build with `criterion` crate, so `criterion` will be removed
# from the `Cargo.toml` temporarily during the sanitizer tests.
ORIGINAL_MANIFEST="Cargo_ori.toml"
mv Cargo.toml $ORIGINAL_MANIFEST
# Delete lines that contain a `criterion` from Cargo.toml.
sed '/criterion/d' $ORIGINAL_MANIFEST > Cargo.toml

sanitizers=("address" "leak" "memory" "thread")
for san in "${sanitizers[@]}"
do
  San="$(tr '[:lower:]' '[:upper:]' <<< ${san:0:1})${san:1}"
  echo -e "\n\nRun ${San}Sanitizer\n------------------------------"
  if [ $san = "leak" ]; then
    echo -e "Skip the test that panics. Leaking memory when the program drops out abnormally is ok."
    options="-- --Z unstable-options --exclude-should-panic"
  fi
  if [ $san = "memory" ]; then
    if [ -z "${toolchain##*darwin*}" ]; then
      echo -e "Skip the MemorySanitizer on Mac OS since it doesn't works with $toolchain"
    else
      echo -e "Skip the MemorySanitizer since it fails to run custom build command for bitflags crate"
    fi
    continue
  fi
  RUSTFLAGS="-Z sanitizer=${san}" cargo test $options
done

mv $ORIGINAL_MANIFEST Cargo.toml