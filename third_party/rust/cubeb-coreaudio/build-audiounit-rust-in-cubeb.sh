#!/bin/sh

git clone --recursive https://github.com/kinetiknz/cubeb.git
cd cubeb/src
git clone https://github.com/ChunMinChang/cubeb-coreaudio-rs.git
cd ../..
mkdir cubeb-build
cd cubeb-build
cmake ../cubeb -DBUILD_RUST_LIBS="ON"
cmake --build .
CUBEB_BACKEND="audiounit-rust" ctest