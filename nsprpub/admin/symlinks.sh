#!/bin/sh
# 
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
# The Original Code is the Netscape Portable Runtime (NSPR).
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

# -----------------------------------------------------------------
# symlinks.sh -- create links from NSPR builds
#
# syntax: symlinks.sh
# 
# Description:
# symlinks.sh creates some symbolic links for NSPR build targets
# that are not actually build, but for which there are NSPR
# binaries suitable for running on the intended target. ... got
# that?
# 
# Suggested use: After copying NSPR binaries to
# /s/b/c/nspr20/<platform> run symlinks.sh to create the links
# for all supported platforms.
#
# The symlinks in this script correspond to the NSPR 4.1.1
# release. Adjust as necessary.
#
# -----------------------------------------------------------------

ln -s SunOS5.6_DBG.OBJ SunOS5.7_DBG.OBJ
ln -s SunOS5.6_OPT.OBJ SunOS5.7_OPT.OBJ

ln -s SunOS5.6_DBG.OBJ SunOS5.8_DBG.OBJ
ln -s SunOS5.6_OPT.OBJ SunOS5.8_OPT.OBJ

ln -s SunOS5.7_64_DBG.OBJ SunOS5.8_64_DBG.OBJ
ln -s SunOS5.7_64_OPT.OBJ SunOS5.8_64_OPT.OBJ

ln -s OSF1V4.0D_DBG.OBJ OSF1V5.0_DBG.OBJ
ln -s OSF1V4.0D_OPT.OBJ OSF1V5.0_OPT.OBJ

ln -s WINNT4.0_DBG.OBJ WINNT5.0_DBG.OBJ
ln -s WINNT4.0_DBG.OBJD WINNT5.0_DBG.OBJD
ln -s WINNT4.0_OPT.OBJ WINNT5.0_OPT.OBJ
# --- end symlinks.sh ---------------------------------------------

