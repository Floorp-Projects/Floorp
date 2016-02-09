#!/bin/bash

usage() {
  echo "Usage: $0 GECKO_DIR GAIA_DIR MULET_TARBALL_URL [--release]"
  echo "  --release"
  echo "    use xpi name with version and platform"
}

if [ "$#" -lt 3 ]; then
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

url_re='^(http|https|ftp)://.*(tar.bz2|zip|dmg)'
if [ -z $3 ] || ! [[ $3 =~ $url_re ]] && ! [ -f $3 ] ; then
  usage
  echo "Third argument should be URL or a path to mulet tarball"
  exit
fi
MULET=$3

RELEASE=
if [ "$4" == "--release" ]; then
  RELEASE=1
fi

if [[ $MULET =~ 'linux-x86_64' ]] ; then
  PLATFORM=linux64
elif [[ $MULET =~ 'linux' ]] ; then
  PLATFORM=linux
elif [[ $MULET =~ 'win32' ]] ; then
  PLATFORM=win32
elif [[ $MULET =~ 'mac' ]] ; then
  PLATFORM=mac64
fi
if [ -z $PLATFORM ]; then
  echo "Unable to compute platform out of mulet package filename: $MULET"
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

set -ex

WORK_DIR=simulator

rm -rf $WORK_DIR/
mkdir -p $WORK_DIR

# If mulet isn't a file, that's an URL and needs to be downloaded first
if [ ! -f $MULET ]; then
  URL=$MULET
  MULET=$WORK_DIR/$(basename $URL)
  wget -O $MULET $URL
fi

# Compute absolute path for all path variables
realpath() {
    [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
}
MULET=$(realpath $MULET)
WORK_DIR=$(realpath $WORK_DIR)
GAIA_DIR=$(realpath $GAIA_DIR)
GECKO_DIR=$(realpath $GECKO_DIR)
SIM_DIR=$(realpath $GECKO_DIR/b2g/simulator)
ADDON_DIR=$(realpath $WORK_DIR/addon)

mkdir -p $ADDON_DIR

# Uncompress the Mulet package
if [[ $MULET =~ .dmg$ ]]; then
  mkdir -p $WORK_DIR/dmg
  if [ "$(uname)" == "Darwin" ]; then
    hdiutil attach -verbose -noautoopen -mountpoint $WORK_DIR/dmg "$MULET"
    # Wait for files to show up
    # hdiutil uses a helper process, diskimages-helper, which isn't always done its
    # work by the time hdiutil exits. So we wait until something shows up in the
    # mnt directory. Due to the async nature of diskimages-helper, the best thing
    # we can do is to make sure the glob() rsync is making can find files.
    i=0
    TIMEOUT=900
    while [ "$(echo $WORK_DIR/dmg/*)" == "$WORK_DIR/dmg/*" ]; do
      if [ $i -gt $TIMEOUT ]; then
        echo "Unable to mount dmg file."
        exit 1
      fi
      sleep 1
      i=$(expr $i + 1)
    done
    # Now we can copy everything out of the $MOUNTPOINT directory into the target directory
    mkdir -p $ADDON_DIR/firefox
    # Try builds are not branded as Firefox
    if [ -d $WORK_DIR/dmg/Nightly.app ]; then
      cp -r $WORK_DIR/dmg/Nightly.app $ADDON_DIR/firefox/FirefoxNightly.app
    else
      cp -r $WORK_DIR/dmg/FirefoxNightly.app $ADDON_DIR/firefox/
    fi
    hdiutil detach $WORK_DIR/DMG
  else
    7z x -o$WORK_DIR/dmg $MULET
    mkdir -p $WORK_DIR/dmg/hfs
    sudo mount -o loop,ro -t hfsplus  $WORK_DIR/dmg/2.hfs $WORK_DIR/dmg/hfs
    mkdir -p $ADDON_DIR/firefox
    # Try builds are not branded as Firefox
    if [ -d $WORK_DIR/dmg/hfs/Nightly.app ]; then
      cp -r $WORK_DIR/dmg/hfs/Nightly.app $ADDON_DIR/firefox/FirefoxNightly.app
    else
      cp -r $WORK_DIR/dmg/hfs/FirefoxNightly.app $ADDON_DIR/firefox/
    fi
    sudo umount $WORK_DIR/dmg/hfs
  fi
  rm -rf $WORK_DIR/dmg
elif [[ $MULET =~ .zip$ ]]; then
  unzip $MULET -d $ADDON_DIR/
elif [[ $MULET =~ .tar.bz2 ]]; then
  (cd $ADDON_DIR/ && tar jxvf $MULET)
else
  echo "Unsupported file extension for $MULET"
  exit
fi

# Build a gaia profile specific to the simulator in order to:
# - disable the FTU
# - set custom prefs to enable devtools debugger server
# - set custom settings to disable lockscreen and screen timeout
# - only ship production apps
NOFTU=1 GAIA_APP_TARGET=production SETTINGS_PATH=$SIM_DIR/custom-settings.json make -j1 -C $GAIA_DIR profile
mv $GAIA_DIR/profile $ADDON_DIR/
cat $SIM_DIR/custom-prefs.js >> $ADDON_DIR/profile/user.js

if [ -f $ADDON_DIR/firefox/application.ini ]; then
  APPLICATION_INI=$ADDON_DIR/firefox/application.ini
elif [ -f $ADDON_DIR/firefox/FirefoxNightly.app/Contents/Resources/application.ini ]; then
  APPLICATION_INI=$ADDON_DIR/firefox/FirefoxNightly.app/Contents/Resources/application.ini
else
  echo "Unable to find application.ini file"
  exit
fi
MOZ_BUILDID=$(sed -n "s/BuildID=\([0-9]\{8\}\)/\1/p" $APPLICATION_INI)
echo "BUILDID $MOZ_BUILDID -- VERSION $VERSION"

XPI_NAME=fxos-simulator-$VERSION.$MOZ_BUILDID-$PLATFORM.xpi
ADDON_ID=fxos_$(echo $VERSION | sed "s/\./_/")_simulator@mozilla.org
ADDON_VERSION=$VERSION.$MOZ_BUILDID
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
    $SIM_DIR/install.rdf.in > $ADDON_DIR/install.rdf

# copy all useful addon file
cp $SIM_DIR/icon.png $ADDON_DIR/
cp $SIM_DIR/icon64.png $ADDON_DIR/

# Create the final zip .xpi file
mkdir -p artifacts
FILE_NAME=fxos-simulator.xpi
if [ "$RELEASE" ]; then
  FILE_NAME=$XPI_NAME
fi
(cd $ADDON_DIR && zip -r - .) > artifacts/$FILE_NAME

