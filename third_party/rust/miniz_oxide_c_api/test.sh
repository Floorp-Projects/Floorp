#!/usr/bin/env bash

set -e

./build.sh --release

mkdir -p bin
g++ tests/miniz_tester.cpp tests/timer.cpp -o bin/miniz_tester -I. -ggdb -O2 -L. -lminiz_oxide_c_api -lutil -ldl -lrt -lpthread -lgcc_s -lc -lm -lrt -lpthread -lutil

for i in 1 2 3 4 5 6
do
    gcc examples/example$i.c -o bin/example$i -lm -I. -ggdb -L. -lminiz_oxide_c_api -lutil -ldl -lrt -lpthread -lgcc_s -lc -lm -lrt -lpthread -lutil
done

mkdir -p test_scratch
if ! test -e "test_scratch/linux-4.8.11"
then
    cd test_scratch
    wget https://cdn.kernel.org/pub/linux/kernel/v4.x/linux-4.8.11.tar.xz -O linux-4.8.11.tar.xz
    tar xf linux-4.8.11.tar.xz
    cd ..
fi

cd test_scratch
echo "-v a"
../bin/miniz_tester -v a linux-4.8.11 > /dev/null
echo "-v -r a"
../bin/miniz_tester -v -r a linux-4.8.11 > /dev/null
echo "-v -b -r a"
../bin/miniz_tester -v -b -r a linux-4.8.11 > /dev/null
echo "-v -a a"
../bin/miniz_tester -v -a a linux-4.8.11/mm > /dev/null

mkdir -p large_file
truncate -s 5G large_file/lf
../bin/miniz_tester -v -a a large_file
