#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#

#
# Config stuff for WINNT 3.51
#
# This makefile defines the following variables:
# OS_CFLAGS and OS_DLLFLAGS.
# It has the following internal variables:
# OS_PROC_CFLAGS and OS_WIN_CFLAGS.

include $(CORE_DEPTH)/coreconf/WIN32.mk

ifeq ($(CPU_ARCH), x386)
	OS_PROC_CFLAGS += -D_X86_
else 
	ifeq ($(CPU_ARCH), MIPS)
		OS_PROC_CFLAGS += -D_MIPS_
	else 
		ifeq ($(CPU_ARCH), ALPHA)
			OS_PROC_CFLAGS += -D_ALPHA_
		endif
	endif
endif

OS_WIN_CFLAGS += -W3
OS_CFLAGS     += -nologo $(OS_WIN_CFLAGS) $(OS_PROC_CFLAGS)
#OS_DLLFLAGS  += -nologo -DLL -PDB:NONE -SUBSYSTEM:WINDOWS
OS_DLLFLAGS   += -nologo -DLL -PDB:NONE -SUBSYSTEM:WINDOWS
#
# Win NT needs -GT so that fibers can work
#
OS_CFLAGS     += -GT
OS_CFLAGS     += -DWINNT
