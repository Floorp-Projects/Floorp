#!/bin/bash

if [ $# -ne 2 ]
then
  echo "Usage: `basename $0` OBJDIR SRCDIR"
  exit $E_BADARGS 
fi

OBJDIR=$1
SRCDIR=$2

echo ---------------------------------------------------
echo OBJDIR = $OBJDIR
echo SRCDIR = $SRCDIR
echo ---------------------------------------------------

pushd $OBJDIR/dist
rm -rf minimo
rm -f minimo.tar.gz

echo Copying over files from OBJDIR

mkdir minimo
cp -pRL bin/libmozjs.so                                    minimo
cp -pRL bin/minimo                                         minimo
cp -pRL bin/libnspr4.so                                    minimo
cp -pRL bin/libplc4.so                                     minimo
cp -pRL bin/libplds4.so                                    minimo
cp -pRL bin/libxpcom.so                                    minimo
cp -pRL bin/libxpcom_core.so                               minimo

cp -pRL bin/libnss3.so                                     minimo
cp -pRL bin/libnssckbi.so                                  minimo
cp -pRL bin/libsmime3.so                                   minimo
cp -pRL bin/libsoftokn3.so                                 minimo
cp -pRL bin/libsoftokn3.chk                                minimo
cp -pRL bin/libssl3.so                                   minimo


mkdir -p minimo/chrome

cp -pRL bin/chrome/classic.jar                             minimo/chrome
cp -pRL bin/chrome/classic.manifest                        minimo/chrome

cp -pRL bin/chrome/en-US.jar                               minimo/chrome
cp -pRL bin/chrome/en-US.manifest                          minimo/chrome

cp -pRL bin/chrome/minimo.jar                              minimo/chrome
cp -pRL bin/chrome/minimo.manifest                         minimo/chrome

cp -pRL bin/chrome/toolkit.jar                             minimo/chrome
cp -pRL bin/chrome/toolkit.manifest                        minimo/chrome

cp -pRL bin/chrome/pippki.jar                              minimo/chrome
cp -pRL bin/chrome/pippki.manifest                         minimo/chrome


mkdir -p minimo/components

cp -pRL bin/components/nsHelperAppDlg.js                   minimo/components
cp -pRL bin/components/nsProgressDialog.js                 minimo/components

cp -pRL bin/components/nsDictionary.js                     minimo/components
cp -pRL bin/components/nsInterfaceInfoToIDL.js             minimo/components
cp -pRL bin/components/nsXmlRpcClient.js                   minimo/components

cp -pRL bin/extensions/spatial-navigation@extensions.mozilla.org/components/* minimo/components

mkdir -p minimo/greprefs
cp -pRL bin/greprefs/*                                     minimo/greprefs

mkdir -p minimo/res
cp -pRL bin/res/*                                          minimo/res
rm -rf minimo/res/samples
rm -rf minimo/res/throbber

mkdir -p minimo/plugins

echo Linking XPT files.

bin/xpt_link minimo/components/all.xpt          bin/components/*.xpt

echo Chewing on chrome

cd minimo/chrome

unzip classic.jar
rm -rf classic.jar
rm -rf skin/classic/communicator
rm -rf skin/classic/editor
rm -rf skin/classic/messenger
rm -rf skin/classic/navigator
zip -r classic.jar skin
rm -rf skin

unzip en-US.jar
rm -rf en-US.jar
rm -rf locale/en-US/communicator
rm -rf locale/en-US/navigator
zip -r en-US.jar locale
rm -rf locale

echo Copying over customized files

popd

pushd $SRCDIR

cp -pRL ../customization/all.js                             $OBJDIR/dist/minimo/greprefs

popd

echo Zipping

tar cvzhf $OBJDIR/dist/minimo_x86_linux.tar.gz $OBJDIR/dist/minimo

echo Done.
