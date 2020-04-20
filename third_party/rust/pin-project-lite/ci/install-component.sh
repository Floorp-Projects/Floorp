#!/bin/bash

set -euo pipefail

component="${1}"

if ! rustup component add "${component}" 2>/dev/null; then
    # If the component is unavailable on the latest nightly,
    # use the latest toolchain with the component available.
    # Refs: https://github.com/rust-lang/rustup-components-history#the-web-part
    target=$(curl -sSf "https://rust-lang.github.io/rustup-components-history/x86_64-unknown-linux-gnu/${component}")
    echo "'${component}' is unavailable on the default toolchain, use the toolchain 'nightly-${target}' instead"

    rustup update "nightly-${target}" --no-self-update
    rustup default "nightly-${target}"

    echo "Query rust and cargo versions:"
    rustup -V
    rustc -V
    cargo -V

    rustup component add "${component}"
fi

echo "Query component versions:"
case "${component}" in
    clippy | miri) cargo "${component}" -V ;;
    rustfmt) "${component}" -V ;;
esac
