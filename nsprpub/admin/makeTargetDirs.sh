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
# makeTargetDirs.sh -- Create target directories for building NSPR
#
# syntax: makeTargetDirs.sh
#
# Description:
# makeTargetDirs.sh creates a set of directories intended for use
# with NSPR's autoconf based build system.
#     
# The enumerated directories are the same as those built for NSPR
# 4.1.1. Adjust as needed.
#
# -----------------------------------------------------------------

mkdir  AIX4.3_64_DBG.OBJ
mkdir  AIX4.3_64_OPT.OBJ
mkdir  AIX4.3_DBG.OBJ
mkdir  AIX4.3_OPT.OBJ
mkdir  HP-UXB.11.00_64_DBG.OBJ
mkdir  HP-UXB.11.00_64_OPT.OBJ
mkdir  HP-UXB.11.00_DBG.OBJ
mkdir  HP-UXB.11.00_OPT.OBJ
mkdir  IRIX6.5_n32_PTH_DBG.OBJ
mkdir  IRIX6.5_n32_PTH_OPT.OBJ
mkdir  Linux2.2_x86_glibc_PTH_DBG.OBJ
mkdir  Linux2.2_x86_glibc_PTH_OPT.OBJ
mkdir  Linux2.4_x86_glibc_PTH_DBG.OBJ
mkdir  Linux2.4_x86_glibc_PTH_OPT.OBJ
mkdir  OSF1V4.0D_DBG.OBJ
mkdir  OSF1V4.0D_OPT.OBJ
mkdir  SunOS5.6_DBG.OBJ
mkdir  SunOS5.6_OPT.OBJ
mkdir  SunOS5.7_64_DBG.OBJ
mkdir  SunOS5.7_64_OPT.OBJ
mkdir  WIN954.0_DBG.OBJ
mkdir  WIN954.0_DBG.OBJD
mkdir  WIN954.0_OPT.OBJ
mkdir  WINNT4.0_DBG.OBJ
mkdir  WINNT4.0_DBG.OBJD
mkdir  WINNT4.0_OPT.OBJ
# --- end makeTargetDirs.sh ---------------------------------------
