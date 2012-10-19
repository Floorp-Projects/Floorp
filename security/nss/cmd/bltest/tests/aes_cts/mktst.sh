#!/bin/sh
for i in 0 1 2 3 4 5
do
    file="aes_cts_$i.txt"
    grep "Key" $file | sed -e 's;Key:;;' | hex > key$i
    grep "IV"  $file | sed -e 's;IV:;;' | hex > iv$i
    grep "Input"  $file | sed -e 's;Input:;;' | hex  > plaintext$i
    grep "Output"  $file | sed -e 's;Output:;;' | hex | btoa > ciphertext$i
done
