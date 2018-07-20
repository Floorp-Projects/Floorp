#!/bin/sh

usage() {
  echo "Usage:"
  echo "\t$0" /path/to/webaudio-benchmarks
  exit 1
}

if [ $# != "1" ]
then
  usage $0
fi

cp $1/* .
