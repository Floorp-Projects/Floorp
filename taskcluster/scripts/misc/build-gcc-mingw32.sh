#!/bin/bash
set -e

# This script is for building a MinGW GCC (and headers) to be used on Linux to compile for Windows.

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$HOME/artifacts

# Do not define root_dir so we build everything to a tmpdir
#root_dir=$HOME_DIR
data_dir=$HOME_DIR/src/build/unix/build-gcc

. $data_dir/build-gcc.sh

gcc_version=6.4.0
gcc_ext=xz
binutils_version=2.27
binutils_ext=bz2
binutils_configure_flags="--target=i686-w64-mingw32"
mingw_version=a39e1aa184206c59982d39b263553a2c58104cef

# GPG keys used to sign GCC (collected from 5.1.0, 5.4.0, 6.4.0)
$GPG --import $data_dir/33C235A34C46AA3FFB293709A328C3A2C3C45C06.key
$GPG --import $data_dir/7F74F97C103468EE5D750B583AB00996FC26A641.key
# GPG key used to sign binutils
$GPG --import $data_dir/EAF1C276A747E9ED86210CBAC3126D3B4AE55E93.key
# GPG key used to sign GMP
$GPG --import $data_dir/343C2FF0FBEE5EC2EDBEF399F3599FF828C67298.key
# GPG key used to sign MPFR
$GPG --import $data_dir/07F3DBBECC1A39605078094D980C197698C3739D.key
# GPG key used to sign MPC
$GPG --import $data_dir/AD17A21EF8AED8F1CC02DBD9F7D5C9BF765C61E3.key

cat > $root_dir/checksums <<EOF
369737ce51587f92466041a97ab7d2358c6d9e1b6490b3940eb09fb0a9a6ac88  binutils-2.27.tar.bz2
850bf21eafdfe5cd5f6827148184c08c4a0852a37ccf36ce69855334d2c914d4  gcc-6.4.0.tar.xz
752079520b4690531171d0f4532e40f08600215feefede70b24fabdc6f1ab160  gmp-5.1.3.tar.bz2
8ceebbf4d9a81afa2b4449113cee4b7cb14a687d7a549a963deb5e2a41458b6b  isl-0.15.tar.bz2
ae79f8d41d8a86456b68607e9ca398d00f8b7342d1d83bcf4428178ac45380c7  mpc-0.8.2.tar.gz
ca498c1c7a74dd37a576f353312d1e68d490978de4395fa28f1cbd46a364e658  mpfr-3.1.5.tar.bz2
EOF

prepare
prepare_mingw
build_binutils
build_gcc_and_mingw

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp $root_dir/mingw32.tar.* $UPLOAD_DIR
