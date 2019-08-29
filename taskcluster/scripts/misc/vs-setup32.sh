VSDIR=vs2017_15.8.4
VSPATH="${MOZ_FETCHES_DIR}/${VSDIR}"
UNIX_VSPATH="$(cd ${MOZ_FETCHES_DIR} && pwd)/${VSDIR}"
SDK_VERSION=10.0.17134.0

export INCLUDE="${VSPATH}/VC/include;${VSPATH}/VC/atlmfc/include;${VSPATH}/SDK/Include/${SDK_VERSION}/ucrt;${VSPATH}/SDK/Include/${SDK_VERSION}/shared;${VSPATH}/SDK/Include/${SDK_VERSION}/um;${VSPATH}/SDK/Include/${SDK_VERSION}/winrt;${VSPATH}/DIA SDK/include"
export LIB="${VSPATH}/VC/lib/x86;${VSPATH}/VC/atlmfc/lib/x86;${VSPATH}/SDK/Lib/${SDK_VERSION}/um/x86;${VSPATH}/SDK/Lib/${SDK_VERSION}/ucrt/x86;${VSPATH}/DIA SDK/lib"
export PATH="${UNIX_VSPATH}/VC/bin/Hostx64/x86:${UNIX_VSPATH}/VC/bin/Hostx64/x64:${UNIX_VSPATH}/VC/bin/Hostx86/x86:${UNIX_VSPATH}/SDK/bin/${SDK_VERSION}/x86:${UNIX_VSPATH}/VC/redist/x86/Microsoft.VC141.CRT:${UNIX_VSPATH}/SDK/Redist/ucrt/DLLs/x86:${UNIX_VSPATH}/DIA SDK/bin:$PATH"
