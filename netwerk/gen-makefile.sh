#!/bin/sh

MODULE_DIR="mozilla/netwerk"
MODULE_DEPTH=`echo $MODULE_DIR | sed -e 's|[^/]||g' -e 's|/|../|g' -e 's|/$||'`

RUN_DIR=`echo $PWD | sed -e 's|.*\('$MODULE_DIR'\)$|\1|'`

if [ -z "$RUN_DIR" -o "$RUN_DIR" != "$MODULE_DIR" ]
then
  echo
  echo "This script needs to be run from $MODULE_DIR"
  echo
  exit -1
fi

if [ -n "$MODULE_DEPTH" ]
then
  cd $MODULE_DEPTH
fi

if [ -f config.status ]
then
  CONFIG_FILES=`cat netwerk/makefiles` ./config.status
else
  echo
  echo "Missing config.status in $PWD"
  echo "Have you run ./configure yet?"
  echo
  exit -1
fi
