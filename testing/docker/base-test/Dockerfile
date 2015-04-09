FROM          quay.io/mozilla/ubuntu:12.04
MAINTAINER    Jonas Finnemann Jensen <jopsen@gmail.com>


COPY sources.list /etc/apt/sources.list

RUN useradd -d /home/worker -s /bin/bash -m worker
RUN apt-get update && apt-get install -y --force-yes \
  alsa-base \
  alsa-utils \
  autoconf2.13 \
  bluez-alsa \
  bluez-alsa:i386 \
  bluez-cups \
  bluez-gstreamer \
  build-essential \
  ca-certificates \
  ccache \
  curl \
  fonts-kacst \
  fonts-kacst-one \
  fonts-liberation \
  fonts-stix \
  fonts-unfonts-core \
  fonts-unfonts-extra \
  fonts-vlgothic \
  g++-multilib \
  gcc-multilib \
  gir1.2-gnomebluetooth-1.0 \
  git \
  gstreamer0.10-alsa \
  gstreamer0.10-ffmpeg \
  gstreamer0.10-plugins-bad \
  gstreamer0.10-plugins-base \
  gstreamer0.10-plugins-good \
  gstreamer0.10-plugins-ugly \
  gstreamer0.10-tools \
  libasound2-dev \
  libasound2-plugins:i386 \
  libcanberra-pulse \
  libcurl4-openssl-dev \
  libdbus-1-dev \
  libdbus-glib-1-dev \
  libdrm-intel1:i386 \
  libdrm-nouveau1a:i386 \
  libdrm-radeon1:i386 \
  libdrm2:i386 \
  libexpat1:i386 \
  libgconf2-dev \
  libgl1-mesa-dri \
  libgl1-mesa-dri:i386 \
  libgl1-mesa-glx \
  libgl1-mesa-glx:i386 \
  libglapi-mesa \
  libglapi-mesa:i386 \
  libglu1-mesa \
  libglu1-mesa:i386 \
  libgnome-bluetooth8 \
  libgstreamer-plugins-base0.10-dev \
  libgstreamer0.10-dev \
  libgtk2.0-dev \
  libiw-dev \
  libllvm2.9 \
  libllvm3.0:i386 \
  libncurses5:i386 \
  libnotify-dev \
  libpulse-dev \
  libpulse-mainloop-glib0:i386 \
  libpulsedsp:i386 \
  libsdl1.2debian:i386 \
  libsox-fmt-alsa \
  libx11-xcb1:i386 \
  libxcb-glx0 \
  libxcb-glx0:i386 \
  libxdamage1:i386 \
  libxfixes3:i386 \
  libxt-dev \
  libxxf86vm1 \
  libxxf86vm1:i386 \
  llvm \
  llvm-2.9 \
  llvm-2.9-dev \
  llvm-2.9-runtime \
  llvm-dev \
  llvm-runtime \
  mercurial \
  mesa-common-dev \
  nano \
  pulseaudio \
  pulseaudio-module-X11 \
  pulseaudio-module-bluetooth \
  pulseaudio-module-gconf \
  python-dev \
  python-pip \
  rlwrap \
  screen \
  software-properties-common \
  sudo \
  tar \
  ttf-arphic-uming \
  ttf-dejavu \
  ttf-indic-fonts-core \
  ttf-kannada-fonts \
  ttf-oriya-fonts \
  ttf-paktype \
  ttf-punjabi-fonts \
  ttf-sazanami-mincho \
  unzip \
  uuid \
  vim \
  wget \
  xvfb \
  yasm \
  zip

ENV NODE_VERSION v0.10.36
RUN cd /usr/local/ && \
    curl https://nodejs.org/dist/$NODE_VERSION/node-$NODE_VERSION-linux-x64.tar.gz | tar -xz --strip-components 1 && \
    node -v

# Set variable normally configured at login, by the shells parent process, these
# are taken from GNU su manual
ENV           HOME          /home/worker
ENV           SHELL         /bin/bash
ENV           USER          worker
ENV           LOGNAME       worker

# Declare default working folder
WORKDIR       /home/worker

# Set a default command useful for debugging
CMD ["/bin/bash", "--login"]
