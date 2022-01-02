#!/bin/sh
for i in 1 2 3 4 5 6
do
    file="test$i.txt"
    grep "KEY = " $file | sed -e 's;KEY = ;;' | hex > key$i
    grep "PLAINTEXT = "  $file | sed -e 's;PLAINTEXT = ;;' | hex  > plaintext$i
    grep "CIPHERTEXT = "  $file | sed -e 's;CIPHERTEXT = ;;' | hex > ciphertext$i.bin
    btoa < ciphertext$i.bin > ciphertext$i
    rm ciphertext$i.bin
done
