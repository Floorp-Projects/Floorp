#!/bin/bash

REV=4560cb830fce63fcffdc4558f4281aaac6a3a1ba

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
mkdir -p rust
touch rust/COMMIT

if [ "$(cat rust/COMMIT)" != "$REV" ]; then
    rm -rf rust
    mkdir rust
    curl -L "https://github.com/rust-lang/rust/archive/${REV}.tar.gz" \
        | tar xz --directory rust --strip-components 1
    echo "$REV" > rust/COMMIT
fi
