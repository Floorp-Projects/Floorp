#!/bin/sh

# Simple wrapper to try and ensure we get the right python version loaded.
which python2.7 > /dev/null

if [ $? -eq 0 ]
then
  python2.7 ./tools/git/eslintvalidate.py
else
  python ./tools/git/eslintvalidate.py
fi

exit $?
