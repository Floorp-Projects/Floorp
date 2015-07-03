#!/bin/bash
# Set up an hg repo for testing
dest=$1
if [ -z "$dest" ]; then
    echo You must specify a destination directory 1>&2
    exit 1
fi

rm -rf $dest
mkdir $dest
cd $dest
hg init

echo "Hello world $RANDOM" > hello.txt
hg add hello.txt
hg commit -m "Adding hello"

hg branch branch2 > /dev/null
echo "So long, farewell" >> hello.txt
hg commit -m "Changing hello on branch"

hg checkout default
echo "Is this thing on?" >> hello.txt
hg commit -m "Last change on default"
