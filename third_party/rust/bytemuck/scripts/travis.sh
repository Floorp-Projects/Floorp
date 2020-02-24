#!/bin/bash

set -e

if [[ "$RUN_MIRI" != "" ]]; then

  cargo clean

  # Install and run the latest version of nightly where miri built successfully.
  # Taken from: https://github.com/rust-lang/miri#running-miri-on-ci

  MIRI_NIGHTLY=nightly-$(curl -s https://rust-lang.github.io/rustup-components-history/x86_64-unknown-linux-gnu/miri)
  echo "Installing latest nightly with Miri: $MIRI_NIGHTLY"
  rustup set profile minimal
  rustup default "$MIRI_NIGHTLY"

  rustup component add miri
  cargo miri setup

  cargo miri test --verbose
  cargo miri test --verbose --no-default-features
  cargo miri test --verbose --all-features

else

  rustup component add clippy
  cargo clippy

  if [[ "$TARGET" != "" ]]; then rustup target install $TARGET; fi

  if [[ "$TARGET" == "wasm32-"* && "$TARGET" != "wasm32-wasi" ]]; then
    cargo-web --version || cargo install cargo-web
    cargo web test --no-default-features $FLAGS --target=$TARGET
    cargo web test $FLAGS --target=$TARGET
    #cargo web test --all-features $FLAGS --target=$TARGET

  elif [[ "$TARGET" == *"-linux-android"* ]]; then
    export PATH=/usr/local/android-sdk/ndk-bundle/toolchains/llvm/prebuilt/linux-x86_64/bin:$PATH
    pushd linux-android
      cargo build --no-default-features --target=$TARGET $FLAGS
      cargo build --target=$TARGET $FLAGS
      #cargo build --all-features --target=$TARGET $FLAGS
      # Don't test, can't run android emulators successfully on travis currently
    popd

  elif [[ "$TARGET" == *"-apple-ios" || "$TARGET" == "wasm32-wasi" ]]; then
    cargo build --no-default-features --target=$TARGET $FLAGS
    cargo build --target=$TARGET $FLAGS
    #cargo build --all-features --target=$TARGET $FLAGS
    # Don't test
    #   iOS simulator setup/teardown is complicated
    #   cargo-web doesn't support wasm32-wasi yet, nor can wasm-pack test specify a target

  elif [[ "$TARGET" == *"-unknown-linux-gnueabihf" ]]; then
    #sudo apt-get update
    #sudo apt-get install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
    pushd generic-cross
      cargo build --no-default-features --target=$TARGET $FLAGS
      cargo build --target=$TARGET $FLAGS
      #cargo build --all-features --target=$TARGET $FLAGS
      # Don't test
    popd

  elif [[ "$TARGET" != "" ]]; then
    pushd generic-cross
      cargo test --no-default-features --target=$TARGET $FLAGS
      cargo test --target=$TARGET $FLAGS
      #cargo test --all-features --target=$TARGET $FLAGS
    popd

  else
    # Push nothing, target host CPU architecture
    cargo test --no-default-features $FLAGS
    cargo test $FLAGS
  fi

fi
