#!/bin/bash -e

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script installs Debian-derived distributions in a chroot environment.
# It can for example be used to have an accurate 32bit build and test
# environment when otherwise working on a 64bit machine.
# N. B. it is unlikely that this script will ever work on anything other than a
# Debian-derived system.

usage() {
  echo "usage: ${0##*/} [-m mirror] [-g group,...] [-s] [-c]"
  echo "-g group,... groups that can use the chroot unauthenticated"
  echo "             Default: 'admin' and current user's group ('$(id -gn)')"
  echo "-m mirror    an alternate repository mirror for package downloads"
  echo "-s           configure default deb-srcs"
  echo "-c           always copy 64bit helper binaries to 32bit chroot"
  echo "-h           this help message"
}

process_opts() {
  local OPTNAME OPTIND OPTERR OPTARG
  while getopts ":g:m:sch" OPTNAME; do
    case "$OPTNAME" in
      g)
        [ -n "${OPTARG}" ] &&
          chroot_groups="${chroot_groups}${chroot_groups:+,}${OPTARG}"
        ;;
      m)
        if [ -n "${mirror}" ]; then
          echo "You can only specify exactly one mirror location"
          usage
          exit 1
        fi
        mirror="$OPTARG"
        ;;
      s)
        add_srcs="y"
        ;;
      c)
        copy_64="y"
        ;;
      h)
        usage
        exit 0
        ;;
      \:)
        echo "'-$OPTARG' needs an argument."
        usage
        exit 1
        ;;
      *)
        echo "invalid command-line option: $OPTARG"
        usage
        exit 1
        ;;
    esac
  done

  if [ $# -ge ${OPTIND} ]; then
    eval echo "Unexpected command line argument: \${${OPTIND}}"
    usage
    exit 1
  fi
}


# Check that we are running as a regular user
[ "$(id -nu)" = root ] && {
  echo "Run this script as a regular user and provide your \"sudo\""           \
       "password if requested" >&2
  exit 1
}
mkdir -p "$HOME/chroot/"

process_opts "$@"

# Error handler
trap 'exit 1' INT TERM QUIT
trap 'sudo apt-get clean; tput bel; echo; echo Failed' EXIT

# Install any missing applications that this script relies on. If these packages
# are already installed, don't force another "apt-get install". That would
# prevent them from being auto-removed, if they ever become eligible for that.
# And as this script only needs the packages once, there is no good reason to
# introduce a hard dependency on things such as dchroot and debootstrap.
dep=
for i in dchroot debootstrap; do
  [ -d /usr/share/doc/"$i" ] || dep="$dep $i"
done
[ -n "$dep" ] && sudo apt-get -y install $dep
sudo apt-get -y install schroot

# Create directory for chroot
sudo mkdir -p /var/lib/chroot

# Find chroot environments that can be installed with debootstrap
targets="$(cd /usr/share/debootstrap/scripts
           ls | grep '^[a-z]*$')"

# Ask user to pick one of the available targets
echo "The following targets are available to be installed in a chroot:"
j=1; for i in $targets; do
  printf '%4d: %s\n' "$j" "$i"
  j=$(($j+1))
done
while :; do
  printf "Which target would you like to install: "
  read n
  [ "$n" -gt 0 -a "$n" -lt "$j" ] >&/dev/null && break
done
j=1; for i in $targets; do
  [ "$j" -eq "$n" ] && { distname="$i"; break; }
  j=$(($j+1))
done

# On x86-64, ask whether the user wants to install x86-32 or x86-64
archflag=
arch=
if [ "$(uname -m)" = x86_64 ]; then
  while :; do
    echo "You are running a 64bit kernel. This allows you to install either a"
    printf "32bit or a 64bit chroot environment. %s"                           \
           "Which one do you want (32, 64) "
    read arch
    [ "${arch}" == 32 -o "${arch}" == 64 ] && break
  done
  [ "${arch}" == 32 ] && archflag="--arch i386" || archflag="--arch amd64"
  arch="${arch}bit"
fi
target="${distname}${arch}"

# Don't overwrite an existing installation
[ -d /var/lib/chroot/"${target}" ] && {
  echo "This chroot already exists on your machine." >&2
  echo "Delete /var/lib/chroot/${target} if you want to start over." >&2
  exit 1
}
sudo mkdir -p /var/lib/chroot/"${target}"

# Offer to include additional standard repositories for Ubuntu-based chroots.
alt_repos=
grep ubuntu.com /usr/share/debootstrap/scripts/"${distname}" >&/dev/null && {
  while :; do
    echo "Would you like to add ${distname}-updates and ${distname}-security "
    echo -n "to the chroot's sources.list (y/n)? "
    read alt_repos
    case "${alt_repos}" in
      y|Y)
        alt_repos="y"
        break
      ;;
      n|N)
        break
      ;;
    esac
  done
}

# Remove stale entry from /etc/schroot/schroot.conf. Entries start
# with the target name in square brackets, followed by an arbitrary
# number of lines. The entry stops when either the end of file has
# been reached, or when the beginning of a new target is encountered.
# This means, we cannot easily match for a range of lines in
# "sed". Instead, we actually have to iterate over each line and check
# whether it is the beginning of a new entry.
sudo sed -ni '/^[[]'"${target%bit}"']$/,${:1;n;/^[[]/b2;b1;:2;p;n;b2};p'       \
         /etc/schroot/schroot.conf

# Download base system. This takes some time
if [ -z "${mirror}" ]; then
 grep ubuntu.com /usr/share/debootstrap/scripts/"${distname}" >&/dev/null &&
   mirror="http://archive.ubuntu.com/ubuntu" ||
   mirror="http://ftp.us.debian.org/debian"
fi
 sudo debootstrap ${archflag} "${distname}" /var/lib/chroot/"${target}"        \
                  "$mirror"

# Add new entry to /etc/schroot/schroot.conf
grep ubuntu.com /usr/share/debootstrap/scripts/"${distname}" >&/dev/null &&
  brand="Ubuntu" || brand="Debian"
if [ -z "${chroot_groups}" ]; then
  chroot_groups="admin,$(id -gn)"
fi
sudo sh -c 'cat >>/etc/schroot/schroot.conf' <<EOF
[${target%bit}]
description=${brand} ${distname} ${arch}
type=directory
directory=/var/lib/chroot/${target}
priority=3
users=root
groups=${chroot_groups}
root-groups=${chroot_groups}
personality=linux$([ "${arch}" != 64bit ] && echo 32)
script-config=script-${target}

EOF

# Set up a special directory that changes contents depending on the target
# that is executing.
sed '/^FSTAB=/s,/mount-defaults",/mount-'"${target}"'",'                       \
         /etc/schroot/script-defaults |
  sudo sh -c 'cat >/etc/schroot/script-'"${target}"
sudo cp /etc/schroot/mount-defaults /etc/schroot/mount-"${target}"
echo "$HOME/chroot/.${target} $HOME/chroot none rw,bind 0 0" |
  sudo sh -c 'cat >>/etc/schroot/mount-'"${target}"
mkdir -p "$HOME/chroot/.${target}"

# Install a helper script to launch commands in the chroot
sudo sh -c 'cat >/usr/local/bin/'"${target%bit}" <<EOF
#!/bin/bash
if [ \$# -eq 0 ]; then
  exec schroot -c ${target%bit} -p
else
  p="\$1"; shift
  exec schroot -c ${target%bit} -p "\$p" -- "\$@"
fi
exit 1
EOF
sudo chown root:root /usr/local/bin/"${target%bit}"
sudo chmod 755 /usr/local/bin/"${target%bit}"

# Add the standard Ubuntu update repositories if requested.
[ "${alt_repos}" = "y" -a \
  -r "/var/lib/chroot/${target}/etc/apt/sources.list" ] &&
sudo sed -i '/^deb .* [^ -]\+ main$/p
             s/^\(deb .* [^ -]\+\) main/\1-security main/
             p
             t1
             d
             :1;s/-security main/-updates main/
             t
             d' "/var/lib/chroot/${target}/etc/apt/sources.list"

# Add a few more repositories to the chroot
[ "${add_srcs}" = "y" -a \
  -r "/var/lib/chroot/${target}/etc/apt/sources.list" ] &&
sudo sed -i 's/ main$/ main restricted universe multiverse/
             p
             t1
             d
          :1;s/^deb/deb-src/
             t
             d' "/var/lib/chroot/${target}/etc/apt/sources.list"

# Update packages
sudo schroot -c "${target%bit}" -p -- /bin/sh -c '
  apt-get update; apt-get -y dist-upgrade' || :

# Install a couple of missing packages
for i in debian-keyring ubuntu-keyring locales sudo; do
  [ -d "/var/lib/chroot/${target}/usr/share/doc/$i" ] ||
    sudo schroot -c "${target%bit}" -p -- apt-get -y install "$i" || :
done

# Configure locales
sudo schroot -c "${target%bit}" -p -- /bin/sh -c '
  l='"${LANG:-en_US}"'; l="${l%%.*}"
  [ -r /etc/locale.gen ] &&
    sed -i "s/^# \($l\)/\1/" /etc/locale.gen
  locale-gen $LANG en_US en_US.UTF-8' || :

# Configure "sudo" package
sudo schroot -c "${target%bit}" -p -- /bin/sh -c '
  egrep '"'^$(id -nu) '"' /etc/sudoers >/dev/null 2>&1 ||
  echo '"'$(id -nu) ALL=(ALL) ALL'"' >>/etc/sudoers'

# Install a few more commonly used packages
sudo schroot -c "${target%bit}" -p -- apt-get -y install                       \
  autoconf automake1.9 dpkg-dev g++-multilib gcc-multilib gdb less libtool     \
  strace

# If running a 32bit environment on a 64bit machine, install a few binaries
# as 64bit. This is only done automatically if the chroot distro is the same as
# the host, otherwise there might be incompatibilities in build settings or
# runtime dependencies. The user can force it with the '-c' flag.
host_distro=$(grep DISTRIB_CODENAME /etc/lsb-release 2>/dev/null | \
  cut -d "=" -f 2)
if [ "${copy_64}" = "y" -o \
    "${host_distro}" = "${distname}" -a "${arch}" = 32bit ] && \
    file /bin/bash 2>/dev/null | grep -q x86-64; then
  readlinepkg=$(sudo schroot -c "${target%bit}" -p -- sh -c \
    'apt-cache search "lib64readline.\$" | sort | tail -n 1 | cut -d " " -f 1')
  sudo schroot -c "${target%bit}" -p -- apt-get -y install                     \
    lib64expat1 lib64ncurses5 ${readlinepkg} lib64z1
  dep=
  for i in binutils gdb strace; do
    [ -d /usr/share/doc/"$i" ] || dep="$dep $i"
  done
  [ -n "$dep" ] && sudo apt-get -y install $dep
  sudo cp /usr/bin/gdb "/var/lib/chroot/${target}/usr/local/bin/"
  sudo cp /usr/bin/ld "/var/lib/chroot/${target}/usr/local/bin/"
  for i in libbfd libpython; do
    lib="$({ ldd /usr/bin/ld; ldd /usr/bin/gdb; } |
           grep "$i" | awk '{ print $3 }')"
    if [ -n "$lib" -a -r "$lib" ]; then
      sudo cp "$lib" "/var/lib/chroot/${target}/usr/lib64/"
    fi
  done
  for lib in libssl libcrypt; do
    sudo cp /usr/lib/$lib* "/var/lib/chroot/${target}/usr/lib64/" || :
  done
fi

# Clean up package files
sudo schroot -c "${target%bit}" -p -- apt-get clean
sudo apt-get clean

# Let the user know what we did
trap '' INT TERM QUIT
trap '' EXIT
cat <<EOF


Successfully installed ${distname} ${arch}

You can run programs inside of the chroot by invoking the "${target%bit}"
command.

Your home directory is shared between the host and the chroot. But I configured
$HOME/chroot to be private to the chroot environment. You can use it
for files that need to differ between environments.
EOF
