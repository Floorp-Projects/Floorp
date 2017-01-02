#!/bin/bash
args=`getopt rmvs $*`
set -- $args
for i
do
  if [ "$i" == "-v" ]; then
    /bin/uname-real -v
  fi
  if [ "$i" == "-r" ]; then
    echo "4.4.16-v7+"
  fi
  if [ "$i" == "-m" ]; then
    echo "armv7l"
  fi
  if [ "$i" == "-s" ]; then
    echo "Linux"
  fi
done