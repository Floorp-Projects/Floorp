#!/bin/bash
set -e

# This script is for building GCC 7 for Linux.

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$HOME/artifacts

root_dir=$HOME_DIR
data_dir=$HOME_DIR/src/build/unix/build-gcc

. $data_dir/build-gcc.sh

gcc_version=7.3.0
gcc_ext=xz
binutils_version=2.28.1
binutils_ext=xz

# GPG key used to sign GCC
$GPG --import $data_dir/13975A70E63C361C73AE69EF6EEB81F8981C74C7.key
# GPG key used to sign binutils
$GPG --import $data_dir/EAF1C276A747E9ED86210CBAC3126D3B4AE55E93.key
# GPG key used to sign GMP
$GPG --import $data_dir/343C2FF0FBEE5EC2EDBEF399F3599FF828C67298.key
# GPG key used to sign MPFR
$GPG --import $data_dir/07F3DBBECC1A39605078094D980C197698C3739D.key
# GPG key used to sign MPC
$GPG --import $data_dir/AD17A21EF8AED8F1CC02DBD9F7D5C9BF765C61E3.key

cat > $HOME_DIR/checksums <<EOF
16328a906e55a3c633854beec8e9e255a639b366436470b4f6245eb0d2fde942  binutils-2.28.1.tar.xz
832ca6ae04636adbb430e865a1451adf6979ab44ca1c8374f61fba65645ce15c  gcc-7.3.0.tar.xz
498449a994efeba527885c10405993427995d3f86b8768d8cdf8d9dd7c6b73e8  gmp-6.1.0.tar.bz2
412538bb65c799ac98e17e8cfcdacbb257a57362acfaaff254b0fcae970126d2  isl-0.16.1.tar.bz2
617decc6ea09889fb08ede330917a00b16809b8db88c29c31bfbb49cbf88ecc3  mpc-1.0.3.tar.gz
d3103a80cdad2407ed581f3618c4bed04e0c92d1cf771a65ead662cc397f7775  mpfr-3.1.4.tar.bz2
EOF

prepare
build_binutils
build_gcc

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp $HOME_DIR/gcc.tar.* $UPLOAD_DIR
