#!/usr/bin/env sh

if [ $# -gt 1 ]
then
  REPO_BASE_URI=$1
  shift
else
  echo
  echo "Usage: sh $0 http://path/to/source [file ...]"
  echo
  exit 1
fi

for file in $@
do
  rm -f $file
  svn cat $REPO_BASE_URI/$file > `basename $file`
done
