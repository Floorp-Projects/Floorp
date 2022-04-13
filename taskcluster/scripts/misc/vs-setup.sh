VSDIR=vs
VSPATH="${MOZ_FETCHES_DIR}/${VSDIR}"
UNIX_VSPATH="$(cd ${MOZ_FETCHES_DIR} && pwd)/${VSDIR}"
VCDIR=vc/tools/msvc/14.16.27023
if [ ! -d "${VSPATH}/${VCDIR}" ]; then
    VCDIR=vc/tools/msvc/14.29.30133
fi
SDKDIR="windows kits/10"
SDK_VERSION=10.0.17134.0
if [ ! -d "${VSPATH}/${SDKDIR}/lib/${SDK_VERSION}" ]; then
    SDK_VERSION=10.0.19041.0
fi

export INCLUDE="${VSPATH}/${VCDIR}/include;${VSPATH}/${VCDIR}/atlmfc/include;${VSPATH}/${SDKDIR}/include/${SDK_VERSION}/ucrt;${VSPATH}/${SDKDIR}/include/${SDK_VERSION}/shared;${VSPATH}/${SDKDIR}/include/${SDK_VERSION}/um;${VSPATH}/${SDKDIR}/include/${SDK_VERSION}/winrt;${VSPATH}/dia sdk/include"
export LIB="${VSPATH}/${VCDIR}/lib/x64;${VSPATH}/${VCDIR}/atlmfc/lib/x64;${VSPATH}/${SDKDIR}/lib/${SDK_VERSION}/um/x64;${VSPATH}/${SDKDIR}/lib/${SDK_VERSION}/ucrt/x64;${VSPATH}/dia sdk/lib/amd64"
export PATH="${UNIX_VSPATH}/${VCDIR}/bin/hostx64/x64:${UNIX_VSPATH}/${VCDIR}/bin/hostx86/x86:${UNIX_VSPATH}/${SDKDIR}/bin/${SDK_VERSION}/x64:${UNIX_VSPATH}/redist/x64/microsoft.vc141.crt:${UNIX_VSPATH}/${SDKDIR}/redist/ucrt/dlls/x64:${UNIX_VSPATH}/dia sdk/bin/amd64:$PATH"
