VSDIR=vs
VSPATH="${MOZ_FETCHES_DIR}/${VSDIR}"
UNIX_VSPATH="$(cd ${MOZ_FETCHES_DIR} && pwd)/${VSDIR}"
VCDIR=VC/Tools/MSVC/14.16.27023
if [ ! -d "${VSPATH}/${VCDIR}" ]; then
    VCDIR=VC/Tools/MSVC/14.29.30133
fi
SDKDIR="Windows Kits/10"
SDK_VERSION=10.0.17134.0
if [ ! -d "${VSPATH}/${SDKDIR}/Lib/${SDK_VERSION}" ]; then
    SDK_VERSION=10.0.19041.0
fi

case "$TARGET" in
aarch64-pc-windows-msvc)
    SDK_CPU=arm64
    ;;
i686-pc-windows-msvc)
    SDK_CPU=x86
    ;;
*)
    SDK_CPU=x64
    ;;
esac

export INCLUDE="${VSPATH}/${VCDIR}/include;${VSPATH}/${VCDIR}/atlmfc/include;${VSPATH}/${SDKDIR}/Include/${SDK_VERSION}/ucrt;${VSPATH}/${SDKDIR}/Include/${SDK_VERSION}/shared;${VSPATH}/${SDKDIR}/Include/${SDK_VERSION}/um;${VSPATH}/${SDKDIR}/Include/${SDK_VERSION}/winrt;${VSPATH}/dia sdk/include"
export LIB="${VSPATH}/${VCDIR}/lib/${SDK_CPU};${VSPATH}/${VCDIR}/atlmfc/lib/${SDK_CPU};${VSPATH}/${SDKDIR}/Lib/${SDK_VERSION}/um/${SDK_CPU};${VSPATH}/${SDKDIR}/Lib/${SDK_VERSION}/ucrt/${SDK_CPU};${VSPATH}/dia sdk/lib/amd64"
export PATH="${UNIX_VSPATH}/${VCDIR}/bin/hostx64/${SDK_CPU}:${UNIX_VSPATH}/${VCDIR}/bin/hostx86/x86:${UNIX_VSPATH}/${SDKDIR}/bin/${SDK_VERSION}/${SDK_CPU}:${UNIX_VSPATH}/redist/${SDK_CPU}/microsoft.vc141.crt:${UNIX_VSPATH}/${SDKDIR}/redist/ucrt/dlls/${SDK_CPU}:${UNIX_VSPATH}/dia sdk/bin/amd64:$PATH"
