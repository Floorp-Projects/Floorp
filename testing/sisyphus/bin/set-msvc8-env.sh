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
export VS80COMNTOOLS='C:\Program Files\Microsoft Visual Studio 8\Common7\Tools\'
export VCINSTALLDIR='C:\Program Files\Microsoft Visual Studio 8\VC'
export FrameworkDir='C:\WINDOWS\Microsoft.NET\Framework'
export FrameworkVersion='v2.0.50727'
export FrameworkSDKDir='C:\Program Files\Microsoft Visual Studio 8\SDK\v2.0'

export VSINSTALLDIR_cyg="/cygdrive/c/Program Files/Microsoft Visual Studio 8"
export VCINSTALLDIR_cyg="/cygdrive/c/Program Files/Microsoft Visual Studio 8/VC"

export DevEnvDir="$VSINSTALLDIR\\Common7\\IDE"
export DevEnvDir_cyg="$VSINSTALLDIR_cyg/Common7/IDE"

export MSVCDir="$VCINSTALLDIR"
export MSVCDir_cyg="$VCINSTALLDIR_cyg"


if [ -d "$MSVCDir_cyg"/PlatformSDK ] ; then
    export PlatformSDKDir="$MSVCDir"\\PlatformSDK
    export PlatformSDKDir_cyg="$MSVCDir_cyg"/PlatformSDK
elif [ -d "/cygdrive/c/Program Files/Microsoft Platform SDK" ] ; then
    export PlatformSDKDir='C:\Program Files\Microsoft Platform SDK'
    export PlatformSDKDir_cyg='/cygdrive/c/Program Files/Microsoft Platform SDK'
else
    echo Can\'t find Platform SDK\!
    break 2
fi

echo Platform SDK Location: $PlatformSDKDir_cyg

if [ ! -f "$MOZ_TOOLS"/lib/libIDL-0.6_s.lib ] ; then
    echo Can\'t find moztools\!  Edit this file and check MOZ_TOOLS path.
fi


export PATH="\
$DevEnvDir_cyg:\
$PlatformSDKDir_cyg/bin:\
$MSVCDir_cyg/bin:\
$VSINSTALLDIR_cyg/Common7/Tools:\
$VSINSTALLDIR_cyg/Common7/Tools/bin:\
$MOZ_TOOLS/bin:\
$PATH"

export INCLUDE="\
$MSVCDir\ATLMFC\INCLUDE;\
$MSVCDir\INCLUDE;\
$PlatformSDKDir\include;\
$FrameworkSDKDir\include;\
$INCLUDE"

export LIB="\
$MSVCDir\ATLMFC\LIB;\
$MSVCDir\LIB;\
$PlatformSDKDir\lib;\
$FrameworkSDKDir\lib;\
$LIB" 

