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
# The Original Code is the Netscape Security Services for Java.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-2000
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

#######################################################################
# Initialize DYNAMIC system library names on some platforms           #
#######################################################################

#
# AIX platforms
#


ifeq ($(OS_ARCH),AIX)
ifeq ($(OS_RELEASE),4.1)
	DLLSYSTEM += -lsvld -lC_r -lC -lpthreads -lc_r -lm /usr/lib/libc.a
else
	DLLSYSTEM += -ldl -lC_r -lC -lpthreads -lc_r -lm /usr/lib/libc.a
endif
endif

#
# HP/UX platforms
#

ifeq ($(OS_ARCH),HP-UX)
	ifeq ($(USE_PTHREADS), 1)
		DLLSYSTEM += -lpthread
	endif
	ifeq ($(PTHREADS_USER), 1)
		DLLSYSTEM += -lpthread
	endif
	ifeq ($(OS_RELEASE),A.09.03)
		DLLSYSTEM += -ldld -L/lib/pa1.1 -lm
	else
		DLLSYSTEM += -ldld -lm -lc 
	endif
endif

#
# IRIX platforms
#

ifeq ($(OS_ARCH), IRIX)
	ifeq ($(USE_PTHREADS), 1)
		DLLSYSTEM += -lpthread
	endif
endif

#
# Linux platforms
#

ifeq ($(OS_ARCH), Linux)
	DLLSYSTEM += -ldl -lpthread -lm
endif

#
# NCR platforms
#

ifeq ($(OS_ARCH), NCR)
	DLLSYSTEM += -lsocket -ldl -lnsl -lc
endif

#
# OSF 1 platforms
#

ifeq ($(OS_ARCH),OSF1)
	ifneq ($(OS_RELEASE),V2.0)
		DLLSYSTEM += -lc_r
	endif
	ifeq ($(USE_PTHREADS), 1)
		DLLSYSTEM += -lpthread -lrt
	endif
	ifeq ($(USE_IPV6), 1)
		DLLSYSTEM += -lip6
	endif
endif

#
# SCO platforms
#

ifeq ($(OS_ARCH), SCO_SV)
	DLLSYSTEM += -lsocket -ldl -lnsl -lc
endif

#
# Solaris platforms
#

ifeq ($(OS_ARCH), SunOS)
	ifneq ($(OS_RELEASE), 4.1.3_U1)
		DLLSYSTEM += -lthread -lposix4 -lsocket -lnsl -lintl -ldl
	endif
endif

#
# UNIXWARE platforms
#

ifeq ($(OS_ARCH), UNIXWARE)
	DLLSYSTEM += -lsocket
endif

#
# Windows platforms
#

ifeq ($(OS_ARCH),WINNT)
	ifneq ($(OS_TARGET),WIN16)
		DLLSYSTEM += wsock32.lib winmm.lib
	endif
endif

