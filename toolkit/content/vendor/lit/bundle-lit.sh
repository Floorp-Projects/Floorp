#!/bin/bash
cp *.patch lit/
cd lit
git apply *.patch
../../../../../mach npm install
../../../../../mach npm run build
cp packages/lit/lit-all.min.js ../../../widgets/vendor/lit.all.mjs
rm -rf * .*
cp ../LICENSE .
