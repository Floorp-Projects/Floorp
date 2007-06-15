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

# Root of Visual Developer Studio Common files.
export VSCommonDir='C:\Program Files\Microsoft Visual Studio\Common'
export VSCommonDir_cyg=`cygpath -u "$VSCommonDir"`

# Root of Visual Developer Studio installed files.
export MSDevDir='C:\Program Files\Microsoft Visual Studio\Common\MSDev98'
export MSDevDir_cyg=`cygpath -u "$MSDevDir"`

# Root of Visual C++ installed files.
export MSVCDir='C:\Program Files\Microsoft Visual Studio\VC98'
export MSVCDir_cyg=`cygpath -u "$MSVCDir"`

# VcOsDir is used to help create either a Windows 95 or Windows NT specific path.
export VcOsDir=WIN95
if [[ "$OS" == "Windows_NT" ]]; then
    export VcOsDir=WINNT
fi

echo Setting environment for using Microsoft Visual C++ tools.

if [[ "$OS" == "Windows_NT" ]]; then
    export PATH="$MSDevDir_cyg/Bin":"$MSVCDir_cyg/Bin":"$VSCommonDir_cyg/Tools/$VcOsDir":"$VSCommonDir_cyg/Tools":"$MOZ_TOOLS/bin":"$PATH"
elif [[ "$OS" == "" ]]; then
    export PATH="$MSDevDir_cyg/Bin":"MSVCDir_cyg/Bin":"$VSCommonDir_cyg/Tools/$VcOsDir":"$VSCommonDir_cyg/Tools":"$windir/SYSTEM":$MOZ_TOOLS/bin:"$PATH"
fi

export INCLUDE="$MSVCDir\\ATL\\Include;$MSVCDir\\Include;$MSVCDir\\MFC\\Include;$INCLUDE"
export LIB="$MSVCDir\\Lib;$MSVCDir\\MFC\\Lib;$LIB"

unset VcOsDir
unset VSCommonDir

