#!/bin/bash

echo "testing make_incremental_updates.py"
python ../make_incremental_updates.py -f testpatchfile.txt

echo "diffing ref.mar and test.mar"
./diffmar.sh ref.mar test.mar
echo "diffing ref-mac.mar and test-mac.mar"
./diffmar.sh ref-mac.mar test-mac.mar
