#!/usr/bin/env bash

set -e

./build.sh --release

mkdir -p bin
g++ tests/miniz_tester.cpp tests/timer.cpp -o bin/miniz_tester -I. -O2 -L. -lminiz_oxide_c_api -lutil -ldl -lrt -lpthread -lgcc_s -lc -lm -lrt -lpthread -lutil
#g++ tests/miniz_tester.cpp tests/timer.cpp miniz/* -o bin/miniz_tester -I. -O2

mkdir -p test_scratch
if ! test -e "test_scratch/linux-4.8.11"
then
    cd test_scratch
    wget https://cdn.kernel.org/pub/linux/kernel/v4.x/linux-4.8.11.tar.xz -O linux-4.8.11.tar.xz
    tar xf linux-4.8.11.tar.xz
    cd ..
fi

cd test_scratch
#../bin/miniz_tester -v a linux-4.8.11
perf stat /usr/bin/time "../bin/miniz_tester" -v a linux-4.8.11
