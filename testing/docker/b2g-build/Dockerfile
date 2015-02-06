FROM          centos:centos6
MAINTAINER    Dustin J. Mitchell <dustin@mozilla.com>

# Run majority of yum installs here so we cache them!
COPY releng.repo /etc/yum.repos.d/releng.repo
RUN yum install -y epel-release && \
  yum update -y && \
  yum makecache && \
  yum install -y \
  # From Building B2G docs
  GConf2-devel \
  alsa-lib-devel \
  autoconf213 \
  bison \
  bzip2 \
  ccache \
  cmake \
  curl \
  curl-devel \
  dbus-devel \
  dbus-glib-devel \
  dbus-glib-devel \
  dbus-python \
  file \
  flex \
  gawk \
  gcc473_0moz1 \
  glibc-devel \
  glibc-devel.i686 \
  glibc-static \
  glibc-static \
  gstreamer-devel \
  gstreamer-plugins-base-devel \
  gtk2-devel \
  install \
  iw \
  libX11-devel \
  libX11-devel.i686 \
  libXrandr.i686 \
  libXt-devel \
  libcurl-devel \
  libnotify-devel \
  libstdc++-static \
  libstdc++-static \
  libstdc++.i686 \
  make \
  mesa-libGL-devel \
  mesa-libGL-devel.i686 \
  mozilla-git \
  mozilla-python27 \
  mozilla-python27-mercurial \
  mpfr-devel.x86_64 \
  ncurses-devel.i686 \
  ncurses:-devel \
  openssh-clients \
  openssl-devel \
  patch \
  perl-DBI \
  perl-Digest-SHA \
  pulseaudio-libs-devel \
  readline-devel.i686 \
  rsync \
  screen \
  subversion-perl \
  tar \
  tcl \
  tk \
  unzip \
  uuid \
  vim \
  wget \
  wireless-tools-devel \
  xorg-x11-server-Xvfb \
  xorg-x11-server-utils \
  xz \
  yasm \
  zip \
  zlib-devel \
  zlib-devel.i686 && \
  # Remove cached packages. Cached package takes up a lot of space and
  # distributing them to workers is wasteful.
  yum clean all

RUN mkdir -p /home/worker/bin
COPY bin/repository-url.py /home/worker/bin/repository-url.py

# Default to mozilla specific python, etc...
ENV PATH /tools/python27-mercurial/bin:/tools/python27/bin:$PATH
ENV PATH /tools/tools/buildfarm/utils:$PATH:/home/worker/bin/
ENV PATH /tools/gcc-4.7.3-0moz1/bin:$PATH

# Use the libstd++ we installed over all others...
env LD_LIBRARY_PATH=/tools/gcc-4.7.3-0moz1/lib64:/tools/gcc-4.7.3-0moz1/lib

RUN useradd -d /home/worker -s /bin/bash -m worker

# Terrible symlink hacks so cc points to the gcc version we intend to use...
RUN ls -lah /tools/gcc-4.7.3-0moz1/bin && ln -s /tools/gcc-4.7.3-0moz1/bin/gcc /tools/gcc-4.7.3-0moz1/bin/cc

# Install node from node's own dist...
ENV NODE_VERSION v0.10.36
RUN cd /usr/local/ && \
    curl https://nodejs.org/dist/$NODE_VERSION/node-$NODE_VERSION-linux-x64.tar.gz | tar -xz --strip-components 1 && \
    node -v

RUN wget --no-check-certificate https://pypi.python.org/packages/source/s/setuptools/setuptools-1.4.2.tar.gz && \
    tar -xvf setuptools-1.4.2.tar.gz && \
    cd setuptools-1.4.2 && python setup.py install && \
    cd - && rm -rf setuptools-1.4.2* && \
    curl https://raw.githubusercontent.com/pypa/pip/master/contrib/get-pip.py | python - && \
    pip install virtualenv

# Generate machine uuid file
RUN dbus-uuidgen --ensure=/var/lib/dbus/machine-id

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
