#!/bin/bash

CWD="$PWD/$(dirname $0)"
OBJ_DIR="$(make platform)"
DIST_DIR="$CWD/../dist/$OBJ_DIR"

# do NSPR things
NSS_GYP=1 make install_nspr

if [ -z "${USE_64}" ]; then
    GYP_PARAMS="-Dtarget_arch=ia32"
fi

# generate NSS build files only if asked for it
if [ -n "${NSS_GYP_GEN}" -o ! -d out/Debug ]; then
    PKG_CONFIG_PATH="$CWD/../nspr/$OBJ_DIR/config" gyp -f ninja $GYP_PARAMS --depth=. nss.gyp
fi
# build NSS
# TODO: only doing this for debug build for now
ninja -C out/Debug/
if [ $? != 0 ]; then
    exit 1
fi

# sign libs
# TODO: this is done every time at the moment.
cd out/Debug/
LD_LIBRARY_PATH=$DIST_DIR/lib/ ./shlibsign -v -i lib/libfreebl3.so
LD_LIBRARY_PATH=$DIST_DIR/lib/ ./shlibsign -v -i lib/libfreeblpriv3.so
LD_LIBRARY_PATH=$DIST_DIR/lib/ ./shlibsign -v -i lib/libnssdbm3.so
LD_LIBRARY_PATH=$DIST_DIR/lib/ ./shlibsign -v -i lib/libsoftokn3.so

# copy files over to the right directory
cp * "$DIST_DIR/bin/"
cp lib/* "$DIST_DIR/lib/"
find . -name "*.a" | xargs cp -t "$DIST_DIR/lib/"
