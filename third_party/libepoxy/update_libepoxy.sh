#!/usr/bin/env bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

mkdir _build_temp
cd _build_temp
# Generate `config.h` and the ninja build files
# To build with glx or x11 remove the meson configuration (-D) options
meson -Dglx=no -Dx11=false
cp "src/config.h" "../src/config.h"

# Generate the source files we need
# Other values are 'x11' and 'glx'
for source_prefix in 'egl' 'gl'
do
  source_dispatch="src/${source_prefix}_generated_dispatch.c"
  ninja "${source_dispatch}"
  cp "${source_dispatch}" "../${source_dispatch}"

  source_header="include/epoxy/${source_prefix}_generated.h"
  ninja "${source_header}"
  cp "${source_header}" "../${source_header}"
done

cd ..
rm -rf _build_temp \
       doc \
       test \
       meson.build \
       registry \
       src/gen_dispatch.py \
       src/meson.build \
       meson_options.txt \
       include/meson.build \
       include/epoxy/meson.build \
       cross \
       .editorconfig

