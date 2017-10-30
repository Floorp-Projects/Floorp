#!/bin/bash
set -ev
if [[ $TRAVIS_RUST_VERSION = stable ]]; then
  wget https://github.com/SimonKagstrom/kcov/archive/master.tar.gz
  tar xzf master.tar.gz
  mkdir kcov-master/build
  cd kcov-master/build
  cmake ..
  make
  make install DESTDIR=../tmp
  cd ../..
  ls target/debug
  ./kcov-master/tmp/usr/local/bin/kcov --coveralls-id=$TRAVIS_JOB_ID --exclude-pattern=/.cargo target/kcov target/debug/httparse-*
fi


