#!/usr/bin/env sh

if [ $# -eq 1 ]
then
    REPO_URI=$1
else
    echo
    echo "Usage: sh $0 http://path/to/source"
    echo
    exit 1
fi

check_out() {
  svn co $REPO_URI src-svn && \
    find src-svn -name .svn | xargs rm -rf
}

copy_over() {
  mkdir -p src && \
    cp -r src-svn/* src/
}

clean_up() {
  rm -rf src-svn
}

check_out && copy_over && clean_up
