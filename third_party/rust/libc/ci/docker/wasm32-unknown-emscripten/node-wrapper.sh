#!/bin/sh

set -e

me=$1
shift
dir=$(dirname $me)
file=$(basename $me)

cd $dir
exec node $file "$@"
