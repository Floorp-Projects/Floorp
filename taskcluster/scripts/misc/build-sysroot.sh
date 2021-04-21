#!/bin/sh

set -x
set -e

arch=$1
shift
SNAPSHOT=20210208T213147Z

case "$arch" in
i386|amd64)
  dist=jessie
  gcc_version=4.9
  ;;
arm64)
  dist=buster
  gcc_version=8
  ;;
*)
  echo "$arch is not supported." >&2
  exit 1
  ;;
esac

packages="
  linux-libc-dev
  libstdc++-${gcc_version}-dev
  libdbus-glib-1-dev
  libdrm-dev
  libfontconfig1-dev
  libfreetype6-dev
  libgconf2-dev
  libgcc-${gcc_version}-dev
  libgtk-3-dev
  libpango1.0-dev
  libpulse-dev
  libx11-xcb-dev
  libxt-dev
  valgrind
  $*
"

# --skip=check/qemu works around https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=981709
# --keyring=... works around https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=981710
# For a sysroot, we don't need everything. Essentially only libraries and headers, as
# well as pkgconfig files. We exclude debug info files and valgrind files that are not
# useful to build.
mmdebstrap \
  --architectures=$arch \
  --variant=extract \
  --include=$(echo $packages | tr ' ' ,) \
  $dist \
  sysroot \
  http://snapshot.debian.org/archive/debian/$SNAPSHOT/ \
  --dpkgopt=path-exclude="*" \
  --dpkgopt=path-include="/lib/*" \
  --dpkgopt=path-include="/lib32/*" \
  --dpkgopt=path-include="/usr/include/*" \
  --dpkgopt=path-include="/usr/lib/*" \
  --dpkgopt=path-include="/usr/lib32/*" \
  --dpkgopt=path-exclude="/usr/lib/debug/*" \
  --dpkgopt=path-exclude="/usr/lib/valgrind/*" \
  --dpkgopt=path-include="/usr/share/pkgconfig/*" \
  --keyring=/usr/share/keyrings/debian-archive-removed-keys.gpg \
  --skip=check/qemu \
  -v

# Adjust symbolic links to link into the sysroot instead of absolute
# paths that end up pointing at the host system.
find sysroot -type l | while read l; do
  t=$(readlink $l)
  case "$t" in
  /*)
    # We have a path in the form "sysroot/a/b/c/d" and we want ../../..,
    # which is how we get from d to the root of the sysroot. For that,
    # we start from the directory containing d ("sysroot/a/b/c"), remove
    # all non-slash characters, leaving is with "///", replace each slash
    # with "../", which gives us "../../../", and then we remove the last
    # slash.
    rel=$(dirname $l | sed 's,[^/],,g;s,/,../,g;s,/$,,')
    ln -sf $rel$t $l
    ;;
  esac
done

tar caf sysroot.tar.zst sysroot

mkdir -p "$UPLOAD_DIR"
mv "sysroot.tar.zst" "$UPLOAD_DIR"
