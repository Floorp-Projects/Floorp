#!/bin/bash -ve

### Firefox Test Setup

yum install -y \
  alsa-utils \
  bluez-alsa \
  bluez-alsa.i686 \
  bluez-cups \
  curl \
  dbus-devel \
  dbus-glib-devel \
  GConf2-devel \
  iw \
  libcurl-devel \
  libdrm.i686 \
  libdrm-devel.i686 \
  libnotify-devel \
  libX11.i686 \
  libX11-devel \
  libxcb.i686 \
  libXdamage.i686 \
  libXfixes.i686 \
  libXxf86vm.i686 \
  llvm \
  llvm-devel \
  ncurses-devel \
  ncurses-devel.i686 \
  openssl-devel \
  pulseaudio-module-gconf \
  pulseaudio-module-x11 \
  pulseaudio \
  yasm \
  python-devel \
  python-pip \
  dbus-python \
  wget \
  ;

# Remove cached packages. Cached package takes up a lot of space and
# distributing them to workers is wasteful.
yum clean all

pip install virtualenv;
mkdir Documents; mkdir Pictures; mkdir Music; mkdir Videos;
hg clone http://hg.mozilla.org/build/mozharness/
echo 'Xvfb :0 -nolisten tcp -screen 0 1600x1200x24 &> /dev/null &' >> .bashrc
chown -R worker:worker /home/worker/* /home/worker/.*
