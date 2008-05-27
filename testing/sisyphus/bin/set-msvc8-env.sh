# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2006.
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Bob Clary <bob@bclary.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

export VSINSTALLDIR='C:\Program Files\Microsoft Visual Studio 8'
export VS80COMNTOOLS="$VSINSTALLDIR\\Common7\\Tools\\"
export VCINSTALLDIR="$VSINSTALLDIR\\VC"
export FrameworkDir='C:\WINDOWS\Microsoft.NET\Framework'
export FrameworkVersion='v2.0.50727'
export FrameworkSDKDir="$VSINSTALLDIR\\SDK\\v2.0"
export DevEnvDir="$VSINSTALLDIR\\Common7\\IDE"
export MSVCDir="$VSINSTALLDIR\\VC"
export PlatformSDKDir="$MSVCDir"\\PlatformSDK

# Windows SDK 6 or later is required after https://bugzilla.mozilla.org/show_bug.cgi?id=412374
# v6.0 - Windows SDK Update for Vista
# v6.1 - Windows SDK for Windows Server 2008
export WindowsSDK6='c:\Program Files\Microsoft SDKs\Windows\v6.0'
export WindowsSDK6_cyg=`cygpath -u "$WindowsSDK6"`

if [[ ! -d "$WindowsSDK6_cyg" ]]; then
    export WindowsSDK6='c:\Program Files\Microsoft SDKs\Windows\v6.1'
    export WindowsSDK6_cyg=`cygpath -u "$WindowsSDK6"`
fi

export VSINSTALLDIR_cyg=`cygpath -u "$VSINSTALLDIR"`
export VS80COMNTOOLS_cyg=`cygpath -u "$VSINSTALLDIR"`
export VCINSTALLDIR_cyg=`cygpath -u "$VCINSTALLDIR"`
export FrameworkDir_cyg=`cygpath -u "$FrameworkDir"`
export DevEnvDir_cyg=`cygpath -u "$DevEnvDir"`
export MSVCDir_cyg=`cygpath -u "$VCINSTALLDIR"`
export PlatformSDKDir_cyg=`cygpath -u "$PlatformSDKDir"`

if [ ! -d "$PlatformSDKDir_cyg" ] ; then
    echo "Can not find Platform SDK at $PlatformSDKDir_cyg"
    break 2
fi

echo Platform SDK Location: $PlatformSDKDir_cyg

if [ ! -d "$WindowsSDK6_cyg" ] ; then
    echo "Can not find Windows SDK 6 at $WindowsSDK6_cyg"
    break 2
fi

echo Windows SDK Location: $WindowsSDK6_cyg

if [ ! -f "$MOZ_TOOLS"/lib/libIDL-0.6_s.lib ] ; then
    echo "Can not find moztools at $MOZ_TOOLS"
    break 2
fi

export PATH="\
$WindowsSDK6_cyg/bin:\
$DevEnvDir_cyg:\
$MSVCDir_cyg/bin:\
$VS80COMNTOOLS_cyg:\
$VS80COMNTOOLS_cyg/bin:\
$PlatformSDKDir_cyg/bin:\
$FrameworkSDKDir_cyg/bin:\
$FrameworkDir_cyg/$FrameworkVersion:\
$MSVCDir/VCPackages:\
$MOZ_TOOLS/bin:\
$PATH"

export INCLUDE="\
$WindowsSDK6\\include;\
$WindowsSDK6\\include\\atl;\
$MSVCDir\\ATLMFC\\INCLUDE;\
$MSVCDir\\INCLUDE;\
$PlatformSDKDir\\include;\
$FrameworkSDKDir\\include;\
$INCLUDE"

export LIB="\
$WindowsSDK6\\lib;\
$MSVCDir\\ATLMFC\\LIB;\
$MSVCDir\\LIB;\
$PlatformSDKDir\\lib;\
$FrameworkSDKDir\\lib;\
$LIB" 

export LIBPATH="\
$FrameworkDir\\$FrameworkVersion;\
$MSVCDir\\ATLMFC\\LIB\
"
# necessary for msys' /etc/profile.d/profile-extrapaths.sh to set properly
if [[ -d "/c/mozilla-build" ]]; then
    export MOZILLABUILD='C:\\mozilla-build\\'
fi

