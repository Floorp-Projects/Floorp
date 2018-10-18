#!/bin/bash
# This configures the environment for running MSVC.  It uses vswhere, the
# registry, and a little knowledge of how MSVC is laid out.

if ! hash vswhere 2>/dev/null; then
    echo "Can't find vswhere on the path, aborting" 1>&2
    exit 1
fi

if ! hash reg 2>/dev/null; then
    echo "Can't find reg on the path, aborting" 1>&2
    exit 1
fi

# Turn a unix-y path into a windows one.
fixpath() {
    if hash cygpath 2>/dev/null; then
        cygpath --unix "$1"
    else # haxx
        echo "$1" | sed -e 's,\\,/,g;s,^\(.\):,/\L\1,;s,/$,,'
    fi
}

# Query the registry.  This takes $1 and tags that on the end of several
# different paths, looking for a value called $2 at that location.
# e.g.,
#   regquery Microsoft\Microsoft SDKs\Windows\v10.0 ProductVersion
# looks for a REG_SZ value called ProductVersion at
#   HKLM\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\v10.0
#   HKLU\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\v10.0
#   etc...
regquery() {
    search=("HKLM\\SOFTWARE\\Wow6432Node" \
            "HKCU\\SOFTWARE\\Wow6432Node" \
            "HKLM\\SOFTWARE" \
            "HKCU\\SOFTWARE")
    for i in "${search[@]}"; do
        r=$(reg query "${i}\\${1}" -v "$2" | sed -e 's/ *'"$2"' *REG_SZ *//;t;d')
        if [ -n "$r" ]; then
            echo "$r"
            return 0
        fi
    done
    return 1
}

VSCOMPONENT=Microsoft.VisualStudio.Component.VC.Tools.x86.x64
vsinstall=$(vswhere -latest -requires "$VSCOMPONENT" -property installationPath)

# Attempt to setup paths if vswhere returns something and VSPATH isn't set.
# Otherwise, assume that the env is setup.
if [[ -n "$vsinstall" && -z "$VSPATH" ]]; then

    case "$target_arch" in
        ia32) m=x86 ;;
        x64) m="$target_arch" ;;
        *)
            echo "No support for target '$target_arch' with MSVC." 1>&2
            exit 1
    esac

    export VSPATH=$(fixpath "$vsinstall")
    export WINDOWSSDKDIR="${VSPATH}/SDK"
    export VCINSTALLDIR="${VSPATH}/VC"

    CRTREG="Microsoft\\Microsoft SDKs\\Windows\\v10.0"
    UniversalCRTSdkDir=$(regquery "$CRTREG" InstallationFolder)
    UniversalCRTSdkDir=$(fixpath "$UniversalCRTSdkDir")
    UCRTVersion=$(regquery "$CRTREG" ProductVersion)
    UCRTVersion=$(cd "${UniversalCRTSdkDir}/include"; ls -d "${UCRTVersion}"* | tail -1)

    VCVER=$(cat "${VCINSTALLDIR}/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt")
    REDISTVER=$(cat "${VCINSTALLDIR}/Auxiliary/Build/Microsoft.VCRedistVersion.default.txt")
    export WIN32_REDIST_DIR="${VCINSTALLDIR}/Redist/MSVC/${REDISTVER}/${m}/Microsoft.VC141.CRT"
    export WIN_UCRT_REDIST_DIR="${UniversalCRTSdkDir}/Redist/ucrt/DLLs/${m}"

    if [ "$m" == "x86" ]; then
        PATH="${PATH}:${VCINSTALLDIR}/Tools/MSVC/${VCVER}/bin/Hostx64/x64"
        PATH="${PATH}:${VCINSTALLDIR}/Tools/MSVC/${VCVER}/bin/Hostx64/x86"
    fi
    PATH="${PATH}:${VCINSTALLDIR}/Tools/MSVC/${VCVER}/bin/Host${m}/${m}"
    PATH="${PATH}:${UniversalCRTSdkDir}/bin/${UCRTVersion}/${m}"
    PATH="${PATH}:${WIN32_REDIST_DIR}"
    export PATH

    INCLUDE="${VCINSTALLDIR}/Tools/MSVC/${VCVER}/ATLMFC/include"
    INCLUDE="${INCLUDE}:${VCINSTALLDIR}/Tools/MSVC/${VCVER}/include"
    INCLUDE="${INCLUDE}:${UniversalCRTSdkDir}/include/${UCRTVersion}/ucrt"
    INCLUDE="${INCLUDE}:${UniversalCRTSdkDir}/include/${UCRTVersion}/shared"
    INCLUDE="${INCLUDE}:${UniversalCRTSdkDir}/include/${UCRTVersion}/um"
    INCLUDE="${INCLUDE}:${UniversalCRTSdkDir}/include/${UCRTVersion}/winrt"
    INCLUDE="${INCLUDE}:${UniversalCRTSdkDir}/include/${UCRTVersion}/cppwinrt"
    export INCLUDE

    LIB="${VCINSTALLDIR}/lib/${m}"
    LIB="${VCINSTALLDIR}/Tools/MSVC/${VCVER}/lib/${m}"
    LIB="${LIB}:${UniversalCRTSdkDir}/lib/${UCRTVersion}/ucrt/${m}"
    LIB="${LIB}:${UniversalCRTSdkDir}/lib/${UCRTVersion}/um/${m}"
    export LIB

    export GYP_MSVS_OVERRIDE_PATH="${VSPATH}"
    export GYP_MSVS_VERSION=$(vswhere -latest -requires "$VSCOMPONENT" -property catalog_productLineVersion)
else
    echo Assuming env setup is already done.
    echo VSPATH=$VSPATH
fi
