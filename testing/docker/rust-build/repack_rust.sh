#!/bin/bash -vex

set -e

# Set verbose options on taskcluster for logging.
test -n "$TASK_ID" && set -x

# Inputs, with defaults

: RUST_URL        ${RUST_URL:=https://static.rust-lang.org/dist/}
: RUST_CHANNEL    ${RUST_CHANNEL:=stable}

: WORKSPACE       ${WORKSPACE:=/home/worker}

die() {
  echo "ERROR: $@"
  exit 1
}

fetch() {
  echo "Fetching $1..."
  curl -Os ${RUST_URL}${1}.asc
  curl -Os ${RUST_URL}${1}
  curl -Os ${RUST_URL}${1}.sha256
  curl -Os ${RUST_URL}${1}.asc.sha256
}

verify() {
  echo "Verifying $1..."
  shasum -c ${1}.sha256
  shasum -c ${1}.asc.sha256
  gpg --verify ${1}.asc ${1}
  keybase verify ${1}.asc
}

fetch_rustc() {
  arch=$1
  echo "Retrieving rustc build for $arch..."
  pkg=$(cat ${IDX} | grep ^rustc | grep $arch)
  test -n "${pkg}" || die "No rustc build for $arch in the manifest."
  test 1 == $(echo ${pkg} | wc -w) ||
    die "Multiple rustc builds for $arch in the manifest."
  fetch ${pkg}
  verify ${pkg}
}

fetch_std() {
  echo "Retrieving rust-std builds for:"
  for arch in $@; do
    echo "  $arch"
  done
  for arch in $@; do
    pkg=$(cat ${IDX} | grep rust-std | grep $arch)
    test -n "${pkg}" || die "No rust-std builds for $arch in the manifest."
    test 1 == $(echo ${pkg} | wc -w) ||
      die "Multiple rust-std builds for $arch in the manifest."
    fetch ${pkg}
    verify ${pkg}
  done
}

install_rustc() {
  pkg=$(cat ${IDX} | grep ^rustc | grep $1)
  base=${pkg%%.tar.*}
  echo "Installing $base..."
  tar xf ${pkg}
  ${base}/install.sh ${INSTALL_OPTS}
  rm -rf ${base}
}

install_std() {
  for arch in $@; do
    for pkg in $(cat ${IDX} | grep rust-std | grep $arch); do
      base=${pkg%%.tar.*}
      echo "Installing $base..."
      tar xf ${pkg}
      ${base}/install.sh ${INSTALL_OPTS}
      rm -rf ${base}
    done
  done
}

check() {
  if test -x ${TARGET}/bin/rustc; then
    file ${TARGET}/bin/rustc
    ${TARGET}/bin/rustc --version
  elif test -x ${TARGET}/bin/rustc.exe; then
    file ${TARGET}/bin/rustc.exe
    ${TARGET}/bin/rustc.exe --version
  else
    die "ERROR: Couldn't fine rustc executable"
  fi
  echo "Installed components:"
  for component in $(cat ${TARGET}/lib/rustlib/components); do
    echo "  $component"
  done
  echo
}

test -n "$TASK_ID" && set -v

linux64="x86_64-unknown-linux-gnu"
linux32="i686-unknown-linux-gnu"

android="arm-linux-androideabi"

win64="x86_64-pc-windows-msvc"
win32="i686-pc-windows-msvc"
win32_i586="i586-pc-windows-msvc"

# Fetch the manifest

IDX=channel-rustc-${RUST_CHANNEL}

fetch ${IDX}
verify ${IDX}

TARGET=rustc
INSTALL_OPTS="--prefix=${PWD}/${TARGET} --disable-ldconfig"

# Repack the linux64 builds.

fetch_rustc $linux64
fetch_std $linux64 $linux32

rm -rf ${TARGET}

install_rustc $linux64
install_std $linux64 $linux32

tar cJf rustc-$linux64-repack.tar.xz ${TARGET}/*
check ${TARGET}

# Repack the win64 builds.

fetch_rustc $win64
fetch_std $win64

rm -rf ${TARGET}

install_rustc $win64
install_std $win64

tar cjf rustc-$win64-repack.tar.bz2 ${TARGET}/*
check ${TARGET}

# Repack the win32 builds.

fetch_rustc $win32
fetch_std $win32 $win32_i586

rm -rf ${TARGET}

install_rustc $win32
install_std $win32 $win32_i586

tar cjf rustc-$win32-repack.tar.bz2 ${TARGET}/*
check ${TARGET}

rm -rf ${TARGET}
