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
# The Original Code is the Netscape security libraries.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1994-2000
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
# Config stuff for WIN95
#
# This makefile defines the following variables:
# OS_CFLAGS and OS_DLLFLAGS.

include $(CORE_DEPTH)/coreconf/WIN32.mk

ifeq ($(CPU_ARCH), x386)
ifndef NS_USE_GCC
	OS_CFLAGS += -W3 -nologo
endif
	DEFINES += -D_X86_
else 
	ifeq ($(CPU_ARCH), MIPS)
		#OS_CFLAGS += -W3 -nologo
		#DEFINES += -D_MIPS_
		OS_CFLAGS  += -W3 -nologo
	else 
		ifeq ($(CPU_ARCH), ALPHA)
			OS_CFLAGS += -W3 -nologo
			DEFINES += -D_ALPHA_=1
		endif
	endif
endif

ifndef NS_USE_GCC
OS_DLLFLAGS += -nologo -DLL -SUBSYSTEM:WINDOWS
ifndef MOZ_DEBUG_SYMBOLS
	OS_DLLFLAGS += -PDB:NONE
endif
endif
DEFINES += -DWIN95

# WINNT uses the lib prefix, Win95 and WinCE don't
NSPR31_LIB_PREFIX = $(NULL)
