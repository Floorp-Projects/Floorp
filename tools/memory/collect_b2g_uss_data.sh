#!/bin/sh

if [ $# -ne 2 ]
then
    echo "Usage: `basename $0` <app title> <output_file.csv>"
    exit 1
fi

while true
do
    sample=`adb shell b2g-procrank | grep "^${1}" | awk '{ print $6 }' | sed 's/.$//'`
    echo "$sample"
    echo "$sample" >> "$2"
    sleep 1
done

