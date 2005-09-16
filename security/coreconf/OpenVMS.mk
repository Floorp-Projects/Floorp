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
# The Original Code is mozilla.org Code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
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

#
# Config stuff for Compaq OpenVMS
#

include $(CORE_DEPTH)/coreconf/UNIX.mk

CC			= cc
CCC			= cxx

RANLIB			= /gnu/bin/true

CPU_ARCH		:= $(shell uname -Wh)

OS_CFLAGS		= -DVMS
OS_CXXFLAGS		= -DVMS

#
# XCFLAGS are the only CFLAGS that are used during a link operation. Defining
# OPTIMIZER in XCFLAGS means that each compilation line gets OPTIMIZER
# included twice, but at least we get OPTIMIZER included in the link
# operations; and OpenVMS needs it!
#
XCFLAGS                        += $(OPTIMIZER)

DSO_LDOPTS	= -shared -auto_symvec
MKSHLIB		= $(CC) $(OPTIMIZER) $(LDFLAGS) $(DSO_LDOPTS)

ifdef MAPFILE
# Add LD options to restrict exported symbols to those in the map file
endif
# Change PROCESS to put the mapfile in the correct format for this platform
PROCESS_MAP_FILE = cp $< $@


#
# Always set CPU_TAG on Linux, OpenVMS, WINCE.
#
CPU_TAG = _$(CPU_ARCH)
