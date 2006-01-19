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
rm -f minimo.zip

echo Copying over files from OBJDIR

mkdir minimo
cp -a bin/minimo.exe                                     minimo
cp -a bin/minimo_runner.exe                              minimo

mkdir -p minimo/chrome

cp -a bin/chrome/classic.jar                             minimo/chrome
cp -a bin/chrome/classic.manifest                        minimo/chrome

cp -a bin/chrome/en-US.jar                               minimo/chrome
cp -a bin/chrome/en-US.manifest                          minimo/chrome

cp -a bin/chrome/minimo.jar                              minimo/chrome
cp -a bin/chrome/minimo.manifest                         minimo/chrome

cp -a bin/chrome/toolkit.jar                             minimo/chrome
cp -a bin/chrome/toolkit.manifest                        minimo/chrome

mkdir -p minimo/components

cp -a bin/components/nsDictionary.js                     minimo/components
cp -a bin/components/nsXmlRpcClient.js                   minimo/components

cp -a bin/extensions/spatial-navigation@extensions.mozilla.org/components/* minimo/components

mkdir -p minimo/greprefs
cp -a bin/greprefs/*                                     minimo/greprefs

mkdir -p minimo/res
cp -a bin/res/*                                          minimo/res
rm -rf minimo/res/samples
rm -rf minimo/res/throbber

mkdir -p minimo/plugins

echo Linking XPT files.

host/bin/host_xpt_link minimo/components/all.xpt          bin/components/*.xpt

echo Chewing on chrome

cd minimo/chrome

unzip toolkit.jar
rm -rf toolkit.jar
rm -rf content/passwordmgr
rm -rf content/mozapps
rm -rf content/help
zip -0 -r toolkit.jar content
rm -rf content

unzip classic.jar
rm -rf classic.jar
rm -rf skin/classic/communicator
rm -rf skin/classic/editor
rm -rf skin/classic/messenger
rm -rf skin/classic/navigator
zip -0 -r classic.jar skin
rm -rf skin

unzip en-US.jar
rm -rf en-US.jar
rm -rf locale/en-US/communicator
rm -rf locale/en-US/navigator
zip -0 -r en-US.jar locale
rm -rf locale

echo Copying over customized files

popd

pushd $SRCDIR

cp -a ../customization/all.js                             $OBJDIR/dist/minimo/greprefs
cp -a ../customization/HelperAppDlg.js                    $OBJDIR/dist/minimo/components

cat ../customization/ua.css.additions >> $OBJDIR/dist/minimo/res/ua.css

#echo Copying ARM shunt lib.  Adjust if you are not building ARM
#
#cp -a ../../build/wince/shunt/build/ARMV4Rel/shunt.dll $OBJDIR/dist/minimo

popd

echo Rebasing

pushd $OBJDIR/dist/minimo

rebase -b 0x0100000 -R . -v *.dll components/*.dll

#
# echo Zipping
#
# zip -r $OBJDIR/dist/minimo.zip $OBJDIR/dist/minimo
#

popd

echo Done.
