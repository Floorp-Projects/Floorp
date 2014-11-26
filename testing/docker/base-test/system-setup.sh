#!/bin/bash -ve

################################### setup.sh ###################################

### Check that we are running as root
test `whoami` == 'root';

### Add worker user
# Minimize the number of things which the build script can do, security-wise
# it's not a problem to let the build script install things with apt-get. But it
# really shouldn't do this, so let's forbid root access.
useradd -d /home/worker -s /bin/bash -m worker;

### Install Useful Packages
# First we update and upgrade to latest versions.
apt-get update;
apt-get upgrade -y;

# Let's install some goodies, ca-certificates is needed for https with hg.
# sudo will be required anyway, but let's make it explicit. It nice to have
# sudo around. We'll also install nano, this is pure bloat I know, but it's
# useful a text editor.
apt-get install -y                  \
  ca-certificates                   \
  sudo                              \
  nano                              \
  tar                               \
  ;

# Then let's install all firefox build dependencies, this are extracted from
# mozboot. See python/mozboot/bin/bootstrap.py in mozilla-central.
apt-get install -y                  \
  autoconf2.13                      \
  build-essential                   \
  ccache                            \
  libasound2-dev                    \
  libcurl4-openssl-dev              \
  libdbus-1-dev                     \
  libdbus-glib-1-dev                \
  libgconf2-dev                     \
  libgstreamer0.10-dev              \
  libgstreamer-plugins-base0.10-dev \
  libgtk2.0-dev                     \
  libiw-dev                         \
  libnotify-dev                     \
  libpulse-dev                      \
  libxt-dev                         \
  mercurial                         \
  git                               \
  mesa-common-dev                   \
  python-dev                        \
  unzip                             \
  uuid                              \
  yasm                              \
  xvfb                              \
  zip                               \
  software-properties-common        \
  ;

### Firefox Test Setup
apt-get install -y \
        alsa-base \
        alsa-utils \
        bluez-alsa \
        bluez-alsa:i386 \
        bluez-cups \
        bluez-gstreamer \
        g++-multilib \
        gcc-multilib \
        gir1.2-gnomebluetooth-1.0 \
        gstreamer0.10-alsa \
        libasound2-plugins:i386 \
        libcanberra-pulse \
        libdrm-intel1:i386 \
        libdrm-nouveau1a:i386 \
        libdrm-radeon1:i386 \
        libdrm2:i386 \
        libexpat1:i386 \
        libgnome-bluetooth8 \
        libllvm2.9 \
        libllvm3.0:i386 \
        libncurses5:i386          \
        libpulse-mainloop-glib0:i386 \
        libpulsedsp:i386 \
        libsdl1.2debian:i386 \
        libsox-fmt-alsa \
        libx11-xcb1:i386 \
        libxcb-glx0:i386 \
        libxcb-glx0 \
        libxdamage1:i386 \
        libxfixes3:i386 \
        libxxf86vm1:i386 \
        libxxf86vm1 \
        llvm \
        llvm-2.9 \
        llvm-2.9-dev \
        llvm-2.9-runtime \
        llvm-dev \
        llvm-runtime \
        pulseaudio-module-bluetooth \
        pulseaudio-module-gconf \
        pulseaudio-module-X11 \
        pulseaudio \
        python-pip

# Install some utilities
curl -sL https://deb.nodesource.com/setup | sudo bash -
apt-get install -y \
  screen \
  vim \
  wget \
  curl \
  rlwrap \
  nodejs \
  ;

# Mozilla-patched mesa libs required for many reftests -- see bug 975034
wget http://puppetagain.pub.build.mozilla.org/data/repos/apt/releng/pool/main/m/mesa/libgl1-mesa-dri_8.0.4-0ubuntu0.6mozilla1_i386.deb
wget http://puppetagain.pub.build.mozilla.org/data/repos/apt/releng/pool/main/m/mesa/libgl1-mesa-dri_8.0.4-0ubuntu0.6mozilla1_amd64.deb
wget http://puppetagain.pub.build.mozilla.org/data/repos/apt/releng/pool/main/m/mesa/libgl1-mesa-glx_8.0.4-0ubuntu0.6mozilla1_i386.deb
wget http://puppetagain.pub.build.mozilla.org/data/repos/apt/releng/pool/main/m/mesa/libgl1-mesa-glx_8.0.4-0ubuntu0.6mozilla1_amd64.deb
wget http://puppetagain.pub.build.mozilla.org/data/repos/apt/releng/pool/main/m/mesa/libglapi-mesa_8.0.4-0ubuntu0.6mozilla1_i386.deb
wget http://puppetagain.pub.build.mozilla.org/data/repos/apt/releng/pool/main/m/mesa/libglapi-mesa_8.0.4-0ubuntu0.6mozilla1_amd64.deb
wget http://puppetagain.pub.build.mozilla.org/data/repos/apt/releng/pool/main/m/mesa/libglu1-mesa_8.0.4-0ubuntu0.6mozilla1_i386.deb
wget http://puppetagain.pub.build.mozilla.org/data/repos/apt/releng/pool/main/m/mesa/libglu1-mesa_8.0.4-0ubuntu0.6mozilla1_amd64.deb
dpkg -i libgl1-mesa-dri_8.0.4-0ubuntu0.6mozilla1_amd64.deb
dpkg -i libgl1-mesa-dri_8.0.4-0ubuntu0.6mozilla1_i386.deb
dpkg -i libglapi-mesa_8.0.4-0ubuntu0.6mozilla1_amd64.deb
dpkg -i libglapi-mesa_8.0.4-0ubuntu0.6mozilla1_i386.deb
dpkg -i libgl1-mesa-glx_8.0.4-0ubuntu0.6mozilla1_i386.deb
dpkg -i libgl1-mesa-glx_8.0.4-0ubuntu0.6mozilla1_amd64.deb
dpkg -i libglu1-mesa_8.0.4-0ubuntu0.6mozilla1_i386.deb
dpkg -i libglu1-mesa_8.0.4-0ubuntu0.6mozilla1_amd64.deb

# Install releng package of nodejs that includes npm
#wget http://puppetagain.pub.build.mozilla.org/data/repos/apt/releng/precise/pool/main/n/nodejs/nodejs_0.10.21-1chl1~precise1_amd64.deb
#dpkg -i nodejs_0.10.21-1chl1~precise1_amd64.deb

### Clean up from setup
# Remove cached .deb packages. Cached package takes up a lot of space and
# distributing them to workers is wasteful.
apt-get clean
rm *.deb

# Remove the setup.sh setup, we don't really need this script anymore, deleting
# it keeps the image as clean as possible.
rm $0; echo "Deleted $0";

################################### setup.sh ###################################
