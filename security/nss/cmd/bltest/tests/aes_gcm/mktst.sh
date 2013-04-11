#!/bin/sh
for i in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17
do
    file="test$i.txt"
    grep K= $file | sed -e 's;K=;;' | hex > key$i
    grep IV=  $file | sed -e 's;IV=;;' | hex > iv$i
    grep "C="  $file | sed -e 's;C=;;' | hex > ciphertext$i.bin
    grep "P="  $file | sed -e 's;P=;;' | hex  > plaintext$i
    grep "A="  $file | sed -e 's;A=;;' | hex  > aad$i
    grep "T="  $file | sed -e 's;T=;;' | hex  >> ciphertext$i.bin
    btoa < ciphertext$i.bin > ciphertext$i
    rm ciphertext$i.bin
done
