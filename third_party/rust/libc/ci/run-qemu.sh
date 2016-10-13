# Initial script which is run inside of all qemu images. The first argument to
# this script (as arranged by the qemu image itself) is the path to where the
# libc crate is mounted.
#
# For qemu images we currently need to install Rust manually as this wasn't done
# by the initial run-travis.sh script
#
# FIXME: feels like run-travis.sh should be responsible for downloading the
#        compiler.

set -ex

ROOT=$1
cp -r $ROOT/libc /tmp/libc
cd /tmp/libc

TARGET=$(cat $ROOT/TARGET)
export CARGO_TARGET_DIR=/tmp

case $TARGET in
  *-openbsd)
    pkg_add rust curl gcc-4.8.4p4
    curl https://static.rust-lang.org/cargo-dist/2015-04-02/cargo-nightly-x86_64-unknown-openbsd.tar.gz | \
      tar xzf - -C /tmp
    export PATH=$PATH:/tmp/cargo-nightly-x86_64-unknown-openbsd/cargo/bin
    export CC=egcc
    ;;

  *)
    echo "Unknown target: $TARGET"
    exit 1
    ;;
esac

exec sh ci/run.sh $TARGET
