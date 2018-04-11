#!/usr/bin/env bash

set -v -e -x

# Prepare build (OCaml packages)
opam init
echo ". /home/worker/.opam/opam-init/init.sh > /dev/null 2> /dev/null || true" >> .bashrc
opam switch -v ${opamv}
opam install ocamlfind batteries sqlite3 fileutils yojson ppx_deriving_yojson zarith pprint menhir ulex process fix wasm stdint

# Get the HACL* code
git clone ${haclrepo} hacl-star
git -C hacl-star checkout ${haclversion}

# Prepare submodules, and build, verify, test, and extract c code
# This caches the extracted c code (pins the HACL* version). All we need to do
# on CI now is comparing the code in this docker image with the one in NSS.
opam config exec -- make -C hacl-star prepare -j$(nproc)
make -C hacl-star -f Makefile.build snapshots/nss -j$(nproc)
KOPTS="-funroll-loops 5" make -C hacl-star/code/curve25519 test -j$(nproc)
make -C hacl-star/code/salsa-family test -j$(nproc)
make -C hacl-star/code/poly1305 test -j$(nproc)

# Cleanup.
rm -rf ~/.ccache ~/.cache
