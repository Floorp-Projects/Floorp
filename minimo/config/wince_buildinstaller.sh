#!/bin/bash

if [ $# -ne 2 ]
then
  echo "Usage: `basename $0` OBJDIR SRCDIR"
  exit $E_BADARGS 
fi

OBJDIR=$1
SRCDIR=$2

INSTALL_DIST=c:/minimo_installer_dist

echo ---------------------------------------------------
echo OBJDIR = $OBJDIR
echo SRCDIR = $SRCDIR
echo ---------------------------------------------------

# move the bits to INSTALL_DIST

pushd $OBJDIR/dist/minimo

rm -rf $INSTALL_DIST/minimo
mkdir $INSTALL_DIST/minimo

cp -aR * $INSTALL_DIST/minimo
popd

# move all the config files into INSTALL_DIST
pushd $SRCDIR/../config/wince/
cp -a * $INSTALL_DIST
popd

pushd $INSTALL_DIST

rm -f minimo.*.DAT minimo.*.cab minimo.log minimo_wince_setup.exe

./cabwiz minimo.inf /err minimo.log /cpu 2577

cat minimo.log

./ezsetup -l english -i minimo_installer.ini -r readme.txt -e eula.txt -o minimo_wince_setup.exe

cp -a minimo_wince_setup.exe $OBJDIR/dist/
