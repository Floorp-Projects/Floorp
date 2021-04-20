#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script installs and configures everything the iris
# testing suite requires.
#!/usr/bin/env bash

set -ve

apt_packages=()

apt_packages+=('apt-utils')
apt_packages+=('autoconf')
apt_packages+=('autoconf-archive')
apt_packages+=('automake')
apt_packages+=('fluxbox')
apt_packages+=('libcairo2-dev')
apt_packages+=('libicu-dev')
apt_packages+=('libjpeg62-turbo-dev')
apt_packages+=('libopencv-contrib-dev')
apt_packages+=('libopencv-dev')
apt_packages+=('libopencv-objdetect-dev')
apt_packages+=('libopencv-superres-dev')
apt_packages+=('libopencv-videostab-dev')
apt_packages+=('libpango1.0-dev')
apt_packages+=('libpng-dev')
apt_packages+=('libpng16-16')
apt_packages+=('libtiff5-dev')
apt_packages+=('libtool')
apt_packages+=('p7zip-full')
apt_packages+=('pkg-config')
apt_packages+=('python3.7-tk')
apt_packages+=('python3.7-dev')
apt_packages+=('python3-pip')
apt_packages+=('scrot')
apt_packages+=('wmctrl')
apt_packages+=('xdotool')
apt_packages+=('xsel')
apt_packages+=('zlib1g-dev')

apt-get update
# This allows packages to be installed without human interaction
export DEBIAN_FRONTEND=noninteractive
apt-get install -y -f "${apt_packages[@]}"

python3.7 -m pip install pipenv
python3.7 -m pip install psutil
python3.7 -m pip install zstandard

mkdir -p /setup
cd /setup

wget http://www.leptonica.org/source/leptonica-1.76.0.tar.gz
tar xopf leptonica-1.76.0.tar.gz
cd leptonica-1.76.0
./configure && make && make install

cd /setup
wget https://github.com/tesseract-ocr/tesseract/archive/4.0.0.tar.gz
tar xopf 4.0.0.tar.gz
cd tesseract-4.0.0
./autogen.sh &&\
./configure --enable-debug &&\
LDFLAGS="-L/usr/local/lib" CFLAGS="-I/usr/local/include" make &&\
make install &&\
make install -langs &&\
ldconfig

cd /setup
wget https://github.com/tesseract-ocr/tessdata/archive/4.0.0.zip
unzip 4.0.0.zip
cd tessdata-4.0.0
ls /usr/local/share/tessdata/
mv ./* /usr/local/share/tessdata/


cd /
rm -rf /setup
rm -rf ~/.ccache

ls ~/.cache

rm -rf ~/.npm

apt-get clean
apt-get autoclean
rm -f "$0"
