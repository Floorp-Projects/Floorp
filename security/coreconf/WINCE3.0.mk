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
# Config stuff for WINCE 3.0 (MS Pocket PC 2002)
#
# CPU_ARCH must already be defined to one of:
#   x86, ARM
#
# This makefile defines the following variables:
# OS_CFLAGS, and OS_DLLFLAGS.

include $(CORE_DEPTH)/coreconf/WINCE.mk

CEVersion  = 300
CePlatform = WIN32_PLATFORM_PSPC=310

ifeq ($(CPU_ARCH), x86)
    DEFINES += -D_X86_ -D_i386_ -Di_386_ -Dx86
    OS_CFLAGS += -Gs8192 -GF
    OS_DLLFLAGS += -machine:IX86
else 
ifeq ($(CPU_ARCH), ARM)
    DEFINES += -DARM -D_ARM_
    OS_DLLFLAGS += -machine:ARM
else 
    include CPU_ARCH_is_undefined
endif
endif

DEFINES += -D_WIN32_WCE=300 -DUNDER_CE=300
DEFINES += -DWIN32_PLATFORM_PSPC=310
DEFINES += -DUNICODE -D_UNICODE
OS_CFLAGS += -W3 -nologo

OS_DLLFLAGS += -DLL 

LINKFLAGS = -nologo -subsystem:windowsce,3.00 \
 -nodefaultlib:libc.lib \
 -nodefaultlib:libcd.lib \
 -nodefaultlib:libcmt.lib \
 -nodefaultlib:libcmtd.lib \
 -nodefaultlib:msvcrt.lib \
 -nodefaultlib:msvcrtd.lib \
 -nodefaultlib:oldnames.lib \
 $(NULL)

ifndef MOZ_DEBUG_SYMBOLS
    LINKFLAGS += -PDB:NONE
endif

LINK    += $(LINKFLAGS)
LDFLAGS += $(LINKFLAGS)

OS_LIBS= coredll.lib corelibc.lib

#DLLBASE = -base:"0x00100000" -stack:0x10000,0x1000 -entry:"_DllMainCRTStartup"
DLLBASE += -align:"4096"

#SUB_SHLOBJS =
#EXTRA_LIBS =
#EXTRA_SHARED_LIBS =
#OS_LIBS=
#LD_LIBS=

#
# Win NT needs -GT so that fibers can work
#
#OS_CFLAGS += -GT
#DEFINES += -DWINNT

# WINNT uses the lib prefix, Win95 and WinCE don't
#NSPR31_LIB_PREFIX = lib
