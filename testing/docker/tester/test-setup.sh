#!/bin/bash -ve

### Firefox Test Setup

apt-get update
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

pip install virtualenv;
mkdir Documents; mkdir Pictures; mkdir Music; mkdir Videos;
hg clone http://hg.mozilla.org/build/mozharness/
echo 'Xvfb :0 -nolisten tcp -screen 0 1600x1200x24 &> /dev/null &' >> .bashrc
chown -R worker:worker /home/worker/* /home/worker/.*

### Clean up from setup
# Remove the setup.sh setup, we don't really need this script anymore, deleting
# it keeps the image as clean as possible.
#rm $0; echo "Deleted $0";
