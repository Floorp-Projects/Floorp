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

# install make-3.81
tar -xjf "${MSYS_SRCDIR}/make-3.81-MSYS-1.0.11-snapshot.tar.bz2" -C "${MSYS_STAGEDIR}/mozilla-build"
# install the uber-new make.exe from Earnie Boyd
cp "${MSYS_SRCDIR}/make.exe" "${MSYS_STAGEDIR}/mozilla-build/make-3.81/bin"

# install UPX
unzip -d "${MSYS_STAGEDIR}/mozilla-build" "${MSYS_SRCDIR}/upx203w.zip"

# install blat
unzip -d "${MSYS_STAGEDIR}/mozilla-build" "${MSYS_SRCDIR}/blat261.full.zip"

# install SVN
unzip -d "${MSYS_STAGEDIR}/mozilla-build" "${MSYS_SRCDIR}/svn-win32-1.4.2.zip"

# install NSIS
unzip -d "${MSYS_STAGEDIR}/mozilla-build" "${MSYS_SRCDIR}/nsis-2.22.zip"

# install unzip
mkdir "${MSYS_STAGEDIR}/mozilla-build/info-zip"
unzip -d "${MSYS_STAGEDIR}/mozilla-build/info-zip" "${MSYS_SRCDIR}/unz552xN.exe"
unzip -d "${MSYS_STAGEDIR}/mozilla-build/info-zip" -o "${MSYS_SRCDIR}/zip232xN.zip"

# build and install libiconv
rm -rf "${MSYS_STAGEDIR}/libiconv-1.11"
tar -xzf "${MSYS_SRCDIR}/libiconv-1.11.tar.gz" -C "${MSYS_STAGEDIR}"

pushd "${MSYS_STAGEDIR}/libiconv-1.11"
./configure --prefix=/local
make
make install DESTDIR="${MSYS_STAGEDIR}/mozilla-build/msys"
popd

# build and install atlthunk_compat
rm -rf "${MSYS_STAGEDIR}/atlthunk_compat"
mkdir "${MSYS_STAGEDIR}/atlthunk_compat"
(cd "${MSYS_SRCDIR}/atlthunk_compat" && nmake -f NMakefile OBJDIR="${MOZ_STAGEDIR}\\atlthunk_compat")
mkdir "${MSYS_STAGEDIR}/mozilla-build/atlthunk_compat"
install "${MSYS_STAGEDIR}/atlthunk_compat/atlthunk.lib" "${MSYS_STAGEDIR}/mozilla-build/atlthunk_compat"

# install moztools-static and wintools-compat
unzip -d "${MSYS_STAGEDIR}/mozilla-build" "${MSYS_SRCDIR}/moztools-static.zip"
rm -f "${MSYS_STAGEDIR}"/mozilla-build/moztools/bin/{gmake.exe,shmsdos.exe,uname.exe}

rm -rf "${MSYS_STAGEDIR}/buildtools"
mkdir -p "${MSYS_STAGEDIR}/mozilla-build/moztools-180compat/bin"
mkdir -p "${MSYS_STAGEDIR}/mozilla-build/moztools-180compat/include/libIDL"
mkdir -p "${MSYS_STAGEDIR}/mozilla-build/moztools-180compat/lib"

unzip -d "${MSYS_STAGEDIR}" "${MSYS_SRCDIR}/wintools-180compat.zip"
cp "${MSYS_STAGEDIR}"/buildtools/windows/bin/x86/{glib-1.2.dll,gmodule-1.2.dll,gthread-1.2.dll,libIDL-0.6.dll,nsinstall.exe} \
    "${MSYS_STAGEDIR}/mozilla-build/moztools-180compat/bin"
for file in "${MSYS_STAGEDIR}"/buildtools/windows/include/*.h; do
    tr -d '\015' < $file > "${MSYS_STAGEDIR}/mozilla-build/moztools-180compat/include"/`basename $file`
done
cp "${MSYS_STAGEDIR}/buildtools/windows/include/libIDL/IDL.h" "${MSYS_STAGEDIR}/mozilla-build/moztools-180compat/include/libIDL"
cp "${MSYS_STAGEDIR}"/buildtools/windows/lib/*.lib \
    "${MSYS_STAGEDIR}/mozilla-build/moztools-180compat/lib"

# Copy various configuration files
cp "${MSYS_SRCDIR}/inputrc" "${MSYS_STAGEDIR}/mozilla-build/msys/etc"
mkdir "${MSYS_STAGEDIR}/mozilla-build/msys/etc/profile.d"
cp "${MSYS_SRCDIR}"/{profile-inputrc.sh,profile-extrapaths.sh,profile-echo.sh,profile-homedir.sh} \
    "${MSYS_STAGEDIR}/mozilla-build/msys/etc/profile.d"

# Copy the batch files that make everything go!
cp "${MSYS_SRCDIR}"/{guess-msvc.bat,start-msvc6.bat,start-msvc71.bat,start-msvc8.bat} "${MSYS_STAGEDIR}/mozilla-build"

# Install autoconf 2.13
tar -xzf "${MSYS_SRCDIR}/autoconf-2.13.tar.gz" -C "${MSYS_STAGEDIR}"
pushd "${MSYS_STAGEDIR}/autoconf-2.13"
./configure --prefix=/local --program-suffix=-2.13
make
make install prefix="${MSYS_STAGEDIR}/mozilla-build/msys/local"
popd

# Install wget
unzip -d "${MSYS_STAGEDIR}/mozilla-build/wget" "${MSYS_SRCDIR}/wget-1.10.2b.zip"

# stage files to make the installer
cp "${MSYS_SRCDIR}"/{license.rtf,installit.nsi} "${MSYS_STAGEDIR}"
unix2dos "${MSYS_STAGEDIR}/license.rtf"
