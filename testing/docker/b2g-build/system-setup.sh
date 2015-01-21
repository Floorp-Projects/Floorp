#!/bin/bash -ve

################################### setup.sh ###################################

### Check that we are running as root
test `whoami` == 'root';

yum install -y                      \
  alsa-lib-devel                    \
  autoconf213                       \
  curl-devel                        \
  dbus-glib-devel                   \
  glibc-static                      \
  gstreamer-devel                   \
  gstreamer-plugins-base-devel      \
  gtk2-devel                        \
  libstdc++-static                  \
  libXt-devel                       \
  mesa-libGL-devel                  \
  pulseaudio-libs-devel             \
  wireless-tools-devel              \
  yasm                              \
  dbus-python                       \
  ;

yum install -y                      \
  libcurl-devel                     \
  openssl-devel                     \
  dbus-devel                        \
  dbus-glib-devel                   \
  GConf2-devel                      \
  iw                                \
  libnotify-devel                   \
  unzip                             \
  uuid                              \
  xorg-x11-server-Xvfb              \
  xorg-x11-server-utils             \
  tar                               \
  tcl                               \
  tk                                \
  unzip                             \
  zip                               \
  ;

# From Building B2G docs
yum install -y                      \
  install                           \
  bison                             \
  bzip2                             \
  ccache                            \
  curl                              \
  flex                              \
  gawk                              \
  gcc-c++                           \
  glibc-devel                       \
  glibc-static                      \
  libstdc++-static                  \
  libX11-devel                      \
  make                              \
  ncurses-devel                     \
  patch                             \
  zlib-devel                        \
  ncurses-devel.i686                \
  readline-devel.i686               \
  zlib-devel.i686                   \
  libX11-devel.i686                 \
  mesa-libGL-devel.i686             \
  glibc-devel.i686                  \
  libstdc++.i686                    \
  libXrandr.i686                    \
  perl-Digest-SHA                   \
  wget                              \
  ;

# Install some utilities, we'll be using nodejs in automation scripts, maybe we
# shouldn't we can clean up later
yum install -y                      \
  screen                            \
  vim                               \
  nodejs                            \
  npm                               \
  ;

# Install mozilla specific packages

# puppetagain packages
base_url="http://puppetagain.pub.build.mozilla.org/data/repos/yum/releng/public/CentOS/6/x86_64/"

# Install Python 2.7, pip, and virtualenv (needed for things like mach)
rpm -ih $base_url/mozilla-python27-2.7.3-1.el6.x86_64.rpm
export PATH="/tools/python27-mercurial/bin:/tools/python27/bin:$PATH"
wget --no-check-certificate https://pypi.python.org/packages/source/s/setuptools/setuptools-1.4.2.tar.gz
tar -xvf setuptools-1.4.2.tar.gz
cd setuptools-1.4.2 && python setup.py install
cd - && rm -rf setuptools-1.4.2*
curl https://raw.githubusercontent.com/pypa/pip/master/contrib/get-pip.py | python -
pip install virtualenv

# Install more recent version of mercurial
rpm -ih $base_url/mozilla-python27-mercurial-3.1.2-1.el6.x86_64.rpm

# Install more recent version of git and dependencies
yum install -y    \
  perl-DBI        \
  subversion-perl \
  ;
rpm -ih $base_url/mozilla-git-1.7.9.4-3.el6.x86_64.rpm

# Install gcc to build gecko
rpm -ih $base_url/gcc473_0moz1-4.7.3-0moz1.x86_64.rpm

### Generate machine uuid file
dbus-uuidgen --ensure=/var/lib/dbus/machine-id

### Clean up from setup
# Remove cached packages. Cached package takes up a lot of space and
# distributing them to workers is wasteful.
yum clean all

# Remove the setup.sh setup, we don't really need this script anymore, deleting
# it keeps the image as clean as possible.
rm $0; echo "Deleted $0";

################################### setup.sh ###################################
