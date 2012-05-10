#!/bin/sh
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
