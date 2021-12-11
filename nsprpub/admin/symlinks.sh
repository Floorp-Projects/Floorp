#!/bin/sh
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

ln -s WINNT4.0_DBG.OBJ WINNT5.0_DBG.OBJ
ln -s WINNT4.0_DBG.OBJD WINNT5.0_DBG.OBJD
ln -s WINNT4.0_OPT.OBJ WINNT5.0_OPT.OBJ
# --- end symlinks.sh ---------------------------------------------

