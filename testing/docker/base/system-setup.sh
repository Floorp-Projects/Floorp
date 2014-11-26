#!/bin/bash -ve

################################### setup.sh ###################################

### Check that we are running as root
test `whoami` == 'root';

### Add worker user
# Minimize the number of things which the build script can do, security-wise
# it's not a problem to let the build script install things with yum. But it
# really shouldn't do this, so let's forbid root access.
useradd -d /home/worker -s /bin/bash -m worker;

# Install extra package mirror
yum install -y epel-release

### Install Useful Packages
# First we update and upgrade to latest versions.
yum update -y

# Let's install some goodies, ca-certificates is needed for https with hg.
# sudo will be required anyway, but let's make it explicit. It nice to have
# sudo around. We'll also install nano, this is pure bloat I know, but it's
# useful a text editor.
yum install -y                      \
  ca-certificates                   \
  sudo                              \
  nano                              \
  ;

# Then let's install all firefox build dependencies, these are extracted from
# mozboot. See python/mozboot/bin/bootstrap.py in mozilla-central.
yum groupinstall -y                 \
  "Development Tools"               \
  "Development Libraries"           \
  "GNOME Software Development"

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
  mercurial                         \
  mesa-libGL-devel                  \
  pulseaudio-libs-devel             \
  wireless-tools-devel              \
  yasm                              \
  python-devel                      \
  dbus-python                       \
  ;

yum install -y                      \
  alsa-lib-devel                    \
  libcurl-devel                     \
  openssl-devel                     \
  dbus-devel                        \
  dbus-glib-devel                   \
  GConf2-devel                      \
  iw                                \
  libnotify-devel                   \
  pulseaudio-libs-devel             \
  libXt-devel                       \
  mercurial                         \
  unzip                             \
  uuid                              \
  xorg-x11-server-Xvfb              \
  tcl                               \
  tk                                \
  ;

# From Building B2G docs
yum install -y                      \
  install                           \
  autoconf213                       \
  bison                             \
  bzip2                             \
  ccache                            \
  curl                              \
  flex                              \
  gawk                              \
  gcc-c++                           \
  git                               \
  glibc-devel                       \
  glibc-static                      \
  libstdc++-static                  \
  libX11-devel                      \
  make                              \
  mesa-libGL-devel                  \
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
  zip                               \
  perl-Digest-SHA                   \
  wget                              \
  ;

# Install some utilities, we'll be using nodejs in automation scripts, maybe we
# shouldn't we can clean up later
yum install -y                      \
  screen                            \
  vim                               \
  nodejs                            \
  ;

# Install mozilla specific packages

# puppetagain packages
base_url="http://puppetagain.pub.build.mozilla.org/data/repos/yum/releng/public/CentOS/6/x86_64/"

# Install gcc to build gecko
rpm -ih $base_url/gcc473_0moz1-4.7.3-0moz1.x86_64.rpm

### Clean up from setup
# Remove cached packages. Cached package takes up a lot of space and
# distributing them to workers is wasteful.
yum clean all

# Remove the setup.sh setup, we don't really need this script anymore, deleting
# it keeps the image as clean as possible.
rm $0; echo "Deleted $0";

################################### setup.sh ###################################
