#!/bin/bash

usage() {
  echo "Usage: $0 GECKO_DIR GAIA_DIR MULET_TARBALL_URL"
}

if [ "$#" -ne 3 ]; then
  usage
  exit
fi

if [ -z $1 ] || [ ! -d $1 ]; then
  usage
  echo "First argument should be path to a gecko checkout"
  exit
fi
GECKO_DIR=$1

if [ -z $2 ] || [ ! -d $2 ]; then
  usage
  echo "Second argument should be path to a gaia checkout"
  exit
fi
GAIA_DIR=$2

url_re='^(http|https|ftp)://.*tar.bz2'
if [ -z $3 ] || ! [[ $3 =~ $url_re ]] ; then
  usage
  echo "Third argument should be URL to mulet tarball"
  exit
fi
BUILD_URL=$3

set -ex

WORKDIR=simulator
SIM_DIR=$GECKO_DIR/b2g/simulator

if [[ $BUILD_URL =~ 'linux-x86_64' ]] ; then
  PLATFORM=linux64
elif [[ $BUILD_URL =~ 'linux' ]] ; then
  PLATFORM=linux
elif [[ $BUILD_URL =~ 'win32' ]] ; then
  PLATFORM=win32
elif [[ $BUILD_URL =~ 'mac64' ]] ; then
  PLATFORM=mac64
fi
if [ -z $PLATFORM ]; then
  echo "Unable to compute platform out of mulet URL: $BUILD_URL"
  exit
fi
echo "PLATFORM: $PLATFORM"

VERSION=$(sed -nE "s/MOZ_B2G_VERSION=([0-9]+\.[0-9]+).*prerelease/\1/p" $GECKO_DIR/b2g/confvars.sh)
re='^[0-9]\.[0-9]$'
if ! [[ $VERSION =~ $re ]] ; then
  echo "Invalid b2g version: $VERSION"
  exit
fi
echo "B2G VERSION: $VERSION"

mkdir -p $WORKDIR

if [ ! -f mulet.tar.bz2 ]; then
  wget -O mulet.tar.bz2 $BUILD_URL
fi
(cd $WORKDIR/ && tar jxvf ../mulet.tar.bz2)

# Not sure it is still useful with mulet...
# remove useless stuff we don't want to ship in simulators
rm -rf $WORKDIR/firefox/gaia $WORKDIR/firefox/B2G.app/Contents/MacOS/gaia $WORKDIR/firefox/B2G.app/Contents/Resources/gaia

# Build a gaia profile specific to the simulator in order to:
# - disable the FTU
# - set custom prefs to enable devtools debugger server
# - set custom settings to disable lockscreen and screen timeout
# - only ship production apps
NOFTU=1 GAIA_APP_TARGET=production SETTINGS_PATH=$SIM_DIR/custom-settings.json make -j1 -C $GAIA_DIR profile
mv $GAIA_DIR/profile $WORKDIR/
cat $SIM_DIR/custom-prefs.js >> $WORKDIR/profile/user.js

APP_BUILDID=$(sed -n "s/BuildID=\([0-9]\{8\}\)/\1/p" $WORKDIR/firefox/application.ini)
echo "BUILDID $APP_BUILDID -- VERSION $VERSION"

XPI_NAME=fxos-simulator-$VERSION.$APP_BUILDID-$PLATFORM.xpi
ADDON_ID=fxos_$(echo $VERSION | sed "s/\./_/")_simulator@mozilla.org
ADDON_VERSION=$VERSION.$APP_BUILDID
ADDON_NAME="Firefox OS $VERSION Simulator"
ADDON_DESCRIPTION="a Firefox OS $VERSION Simulator"

echo "ADDON_ID: $ADDON_ID"
echo "ADDON_VERSION: $ADDON_VERSION"

FTP_ROOT_PATH=/pub/mozilla.org/labs/fxos-simulator
UPDATE_PATH=$VERSION/$PLATFORM
UPDATE_LINK=https://ftp.mozilla.org$FTP_ROOT_PATH/$UPDATE_PATH/$XPI_NAME
UPDATE_URL=https://ftp.mozilla.org$FTP_ROOT_PATH/$UPDATE_PATH/update.rdf

# Replace all template string in install.rdf
sed -e "s/@ADDON_VERSION@/$ADDON_VERSION/" \
    -e "s/@ADDON_ID@/$ADDON_ID/" \
    -e "s#@ADDON_UPDATE_URL@#$UPDATE_URL#" \
    -e "s/@ADDON_NAME@/$ADDON_NAME/" \
    -e "s/@ADDON_DESCRIPTION@/$ADDON_DESCRIPTION/" \
    $SIM_DIR/install.rdf.in > $WORKDIR/install.rdf

# copy all useful addon file
cp $SIM_DIR/icon.png $WORKDIR/
cp $SIM_DIR/icon64.png $WORKDIR/

mkdir -p artifacts
(cd $WORKDIR && zip -r - .) > artifacts/$XPI_NAME

