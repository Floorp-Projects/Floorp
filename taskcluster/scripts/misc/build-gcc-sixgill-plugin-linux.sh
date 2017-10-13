#!/bin/bash

set -e
set -x

# This script is for building the sixgill GCC plugin for Linux. It relies on
# the gcc checkout because it needs to recompile gmp and the gcc build script
# determines the version of gmp to download.

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$HOME/artifacts

root_dir=$HOME_DIR
build_dir=$HOME_DIR/src/build
data_dir=$HOME_DIR/src/build/unix/build-gcc

# Download and unpack upstream toolchain artifacts (ie, the gcc binary).
. $(dirname $0)/tooltool-download.sh

gcc_version=4.9.4
gcc_ext=bz2
binutils_version=2.25.1
binutils_ext=bz2
sixgill_rev=59b74c2e21bd
sixgill_repo=https://hg.mozilla.org/users/sfink_mozilla.com/sixgill

. $data_dir/build-gcc.sh

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
b5b14added7d78a8d1ca70b5cb75fef57ce2197264f4f5835326b0df22ac9f22  binutils-2.25.1.tar.bz2
02500a4edd14875f94fe84cbeda4290425cb0c1c2474c6f75d75a303d64b4196  cloog-0.18.1.tar.gz
6c11d292cd01b294f9f84c9a59c230d80e9e4a47e5c6355f046bb36d4f358092  gcc-4.9.4.tar.bz2
752079520b4690531171d0f4532e40f08600215feefede70b24fabdc6f1ab160  gmp-5.1.3.tar.bz2
f4b3dbee9712850006e44f0db2103441ab3d13b406f77996d1df19ee89d11fb4  isl-0.12.2.tar.bz2
ae79f8d41d8a86456b68607e9ca398d00f8b7342d1d83bcf4428178ac45380c7  mpc-0.8.2.tar.gz
ca498c1c7a74dd37a576f353312d1e68d490978de4395fa28f1cbd46a364e658  mpfr-3.1.5.tar.bz2
EOF

# Download GCC + related, and unpack.
prepare

export TMPDIR=${TMPDIR:-/tmp/}
export gcc_bindir=$root_dir/src/gcc/bin
export gmp_prefix=/tools/gmp
export gmp_dir=$root_dir$gmp_prefix

prepare_sixgill() {(
    cd $root_dir
    hg clone -r $sixgill_rev $sixgill_repo || ( cd sixgill && hg update -r $sixgill_rev )
)}

build_gmp() {
    if ! [ -x $gcc_bindir/gcc ]; then
        echo "GCC not found in $gcc_bindir/gcc" >&2
        exit 1
    fi

    # The sixgill plugin uses some gmp symbols, including some not exported by
    # cc1/cc1plus. So link the plugin statically to libgmp. Except that the
    # default static build does not have -fPIC, and will result in a relocation
    # error, so build our own. This requires the gcc and related source to be
    # in $root_dir/gcc-$gcc_version.

    mkdir $root_dir/gmp-objdir || true
    (
        cd $root_dir/gmp-objdir
        $root_dir/gcc-$gcc_version/gmp/configure --disable-shared --with-pic --prefix=$gmp_prefix
        make -j8
        make install DESTDIR=$root_dir
    )
}

build_sixgill() {(
    cd $root_dir/sixgill
    export CC=$gcc_bindir/gcc
    export CXX=$gcc_bindir/g++
    export TARGET_CC=$CC
    export CPPFLAGS=-I$gmp_dir/include
    export EXTRA_LDFLAGS=-L$gmp_dir/lib
    export HOST_CFLAGS=$CPPFLAGS

    ./release.sh --build-and-package --with-gmp=$gmp_dir
    tarball=$(ls -td *-sixgill | head -1)/sixgill.tar.xz
    cp $tarball $root_dir/sixgill.tar.xz
)}

prepare_sixgill
build_gmp
build_sixgill

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp $HOME_DIR/sixgill.tar.* $UPLOAD_DIR
