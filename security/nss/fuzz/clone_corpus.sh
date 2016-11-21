#!/bin/sh

cd $(dirname $0)

mkdir tmp/
git clone --no-checkout --depth 1 https://github.com/mozilla/nss-fuzzing-corpus tmp/
(cd tmp && git reset --hard master)

mkdir -p corpus
cp -r tmp/* corpus
rm -Rf tmp/
