#!/bin/bash
set -x -e -v

# This script is for extracting python bianry for windows from setup file.

ARTIFACT_NAME=win64-cpython.tar.zst
PYTHON_INSTALLER=`echo $MOZ_FETCHES_DIR/python-3.*-amd64.exe`
WINE=$MOZ_FETCHES_DIR/wine/bin/wine

cabextract $PYTHON_INSTALLER

tardir=python
mkdir $tardir
pushd $tardir
msiextract ../*
rm -f api-ms-win-*

# bundle pip
$WINE python.exe -m ensurepip
$WINE python.exe -m pip install --upgrade pip==23.0
$WINE python.exe -m pip install --only-binary ':all:' -r ${GECKO_PATH}/build/psutil_requirements.txt -r ${GECKO_PATH}/build/zstandard_requirements.txt

# extra symlinks to have a consistent install with Linux and OSX
ln -s python.exe python3.exe
chmod u+x python3.exe

ln -s ./Scripts/pip3.exe pip3.exe
chmod u+x pip3.exe


popd

tar caf `basename ${TOOLCHAIN_ARTIFACT}` ${tardir}

mkdir -p $UPLOAD_DIR
mv `basename ${TOOLCHAIN_ARTIFACT}` $UPLOAD_DIR
