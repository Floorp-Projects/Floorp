#!/bin/bash -xe

pushd testing/cross/
docker build -t local_cross:x86_64-unknown-linux-gnu -f x86_64-unknown-linux-gnu.Dockerfile .
docker build -t local_cross:powerpc64le-unknown-linux-gnu -f powerpc64le-unknown-linux-gnu.Dockerfile .
popd

cross test --target x86_64-unknown-linux-gnu
cross build --target x86_64-unknown-netbsd
cross build --target powerpc64le-unknown-linux-gnu
cross build --target x86_64-pc-windows-gnu
