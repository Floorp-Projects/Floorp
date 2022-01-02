#!/bin/sh
for i in 0 1 2
do
    file="aes_ctr_$i.txt"
    grep Key $file | sed -e 's;Key=;;' | hex > key$i
    grep "Init. Counter"  $file | sed -e 's;Init. Counter=;;' | hex > iv$i
    grep "Ciphertext"  $file | sed -e 's;Ciphertext=;;' | hex | btoa > ciphertext$i
    grep "Plaintext"  $file | sed -e 's;Plaintext=;;' | hex  > plaintext$i
done
