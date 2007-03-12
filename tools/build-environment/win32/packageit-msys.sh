set -e

# This script expects these to be absolute paths in win32 format
if test -z "$MOZ_SRCDIR"; then
    echo "This script should be run from packageit.py (MOZ_SRCDIR missing)."
    exit 1
fi
if test -z "$MOZ_STAGEDIR"; then
    echo "This script should be run from packageit.py (MOZ_STAGEDIR missing)."
    exit 1
fi

MSYS_SRCDIR=$(cd "$MOZ_SRCDIR" && pwd)
MSYS_STAGEDIR=$(cd "$MOZ_STAGEDIR" && pwd)

rm -rf "${MSYS_STAGEDIR}"{MIME_Base64-3.07,Time-HiRes-1.9707,make-3.81.90,cvs-1.11.22}

tar -xzf "${MSYS_SRCDIR}/MIME-Base64-3.07.tar.gz" -C "${MSYS_STAGEDIR}"
pushd "${MSYS_STAGEDIR}/MIME-Base64-3.07"
perl Makefile.pl
make LD="gcc -shared"
make test
make install PREFIX="${MSYS_STAGEDIR}/mozilla-build/msys"
popd

tar -xzf "${MSYS_SRCDIR}/Time-HiRes-1.9707.tar.gz" -C "${MSYS_STAGEDIR}"
pushd "${MSYS_STAGEDIR}/Time-HiRes-1.9707"
perl Makefile.pl
make LD="gcc -shared"
# make test fails because of misconfigured virtual internal timers :-(
make install PREFIX="${MSYS_STAGEDIR}/mozilla-build/msys"
popd

tar -xjf "${MSYS_SRCDIR}/make-3.81.90.tar.bz2" -C "${MSYS_STAGEDIR}"
pushd "${MSYS_STAGEDIR}/make-3.81.90"
patch -p0 < "${MSYS_SRCDIR}/make-msys.patch"
./configure --prefix=/local
make
make install prefix="${MSYS_STAGEDIR}/mozilla-build/msys/local"
popd

tar -xjf "${MSYS_SRCDIR}/cvs-1.11.22.tar.bz2" -C "${MSYS_STAGEDIR}"
pushd "${MSYS_STAGEDIR}/cvs-1.11.22"
./configure --prefix=/local
make
make install prefix="${MSYS_STAGEDIR}/mozilal-build/msys/local"
popd

# In order for this to actually work, we now need to rebase
# the DLL. Since I can't figure out how to rebase just one
# DLL to avoid conflicts with a set of others, we just
# rebase them all!

find "${MSYS_STAGEDIR}/mozilla-build/lib" -name "*.dll" | \
  xargs rebase -d -b 60000000
