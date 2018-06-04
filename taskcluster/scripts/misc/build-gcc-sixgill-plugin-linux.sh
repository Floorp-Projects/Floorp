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

gcc_version=6.4.0
gcc_ext=xz
binutils_version=2.28.1
binutils_ext=xz
sixgill_rev=ab06fc42cf0f
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
16328a906e55a3c633854beec8e9e255a639b366436470b4f6245eb0d2fde942  binutils-2.28.1.tar.xz
850bf21eafdfe5cd5f6827148184c08c4a0852a37ccf36ce69855334d2c914d4  gcc-6.4.0.tar.xz
752079520b4690531171d0f4532e40f08600215feefede70b24fabdc6f1ab160  gmp-5.1.3.tar.bz2
8ceebbf4d9a81afa2b4449113cee4b7cb14a687d7a549a963deb5e2a41458b6b  isl-0.15.tar.bz2
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
    export PATH="$gcc_bindir:$PATH"
    export LD_LIBRARY_PATH="${gcc_bindir%/bin}/lib64"
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
