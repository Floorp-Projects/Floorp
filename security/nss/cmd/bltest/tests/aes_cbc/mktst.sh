#!/bin/sh
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
do
    file="test$i.txt"
    grep "KEY = " $file | sed -e 's;KEY = ;;' | hex > key$i
    grep "IV = "  $file | sed -e 's;IV = ;;' | hex > iv$i
    grep "PLAINTEXT = "  $file | sed -e 's;PLAINTEXT = ;;' | hex  > plaintext$i
    grep "CIPHERTEXT = "  $file | sed -e 's;CIPHERTEXT = ;;' | hex > ciphertext$i.bin
    btoa < ciphertext$i.bin > ciphertext$i
    rm ciphertext$i.bin
done
