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

### Clean up from setup
# Remove cached packages. Cached package takes up a lot of space and
# distributing them to workers is wasteful.
yum clean all

# Remove the setup.sh setup, we don't really need this script anymore, deleting
# it keeps the image as clean as possible.
rm $0; echo "Deleted $0";

