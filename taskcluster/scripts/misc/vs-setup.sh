VSDIR=vs2017_15.8.4
VSPATH="${MOZ_FETCHES_DIR}/${VSDIR}"
UNIX_VSPATH="$(cd ${MOZ_FETCHES_DIR} && pwd)/${VSDIR}"
SDK_VERSION=10.0.17134.0

export INCLUDE="${VSPATH}/VC/include;${VSPATH}/VC/atlmfc/include;${VSPATH}/SDK/Include/${SDK_VERSION}/ucrt;${VSPATH}/SDK/Include/${SDK_VERSION}/shared;${VSPATH}/SDK/Include/${SDK_VERSION}/um;${VSPATH}/SDK/Include/${SDK_VERSION}/winrt;${VSPATH}/DIA SDK/include"
export LIB="${VSPATH}/VC/lib/x64;${VSPATH}/VC/atlmfc/lib/x64;${VSPATH}/SDK/lib/${SDK_VERSION}/um/x64;${VSPATH}/SDK/lib/${SDK_VERSION}/ucrt/x64;${VSPATH}/DIA SDK/lib/amd64"
export PATH="${UNIX_VSPATH}/VC/bin/Hostx64/x64:${UNIX_VSPATH}/VC/bin/Hostx86/x86:${UNIX_VSPATH}/SDK/bin/${SDK_VERSION}/x64:${UNIX_VSPATH}/redist/x64/Microsoft.VC141.CRT:${UNIX_VSPATH}/SDK/Redist/ucrt/DLLs/x64:${UNIX_VSPATH}/DIA SDK/bin/amd64:$PATH"
