#!/usr/bin/env bash

mkdir -p bin

rm bin/libminiz.a
gcc -O2 -c -fPIC miniz/*.c
for f in *.o ; do mv "$f" "c_$f" ; done

for f in c_*.o ; do objcopy "$f" --redefine-syms redefine.txt; done

gcc -O2 -c -fPIC miniz_stub/*.c
ar rsc -o bin/libminiz.a *.o

rm *.o
