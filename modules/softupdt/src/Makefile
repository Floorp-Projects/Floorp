#!gmake
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.

DEPTH		= ../../..

DIRS		= nsdiff nspatch

include $(DEPTH)/config/config.mk

MODULE		= softupdate
LIBRARY_NAME	= softupdate

REQUIRES	= softupdt js java net dbm nspr img util layer pref \
		  jar security lay style libreg network progress base xpcom caps zlib plugin oji ojiimpl

INCLUDES	+= -I$(DEPTH)/base/src

ifeq ($(MOZ_OJI),1)
REQUIRES	+= oji npj
endif

CSRCS		= \
		  softupdt.c \
		  su_trigger.c \
		  su_version.c \
		  su_patch.c \
		  su_folderspec.c \
		  su_instl.c \
		  nsFolderSpec.c \
		  nsVersionInfo.c \
		  nsInstallDelete.c \
		  nsInstallExecute.c \
		  nsWinProfile.c \
		  nsInstallFile.c \
		  nsWinProfileItem.c \
		  nsInstallPatch.c \
		  nsWinReg.c \
		  nsSUError.c \
		  nsWinRegItem.c \
		  nsSoftwareUpdate.c \
		  nsWinRegValue.c \
		  $(NULL)

ifneq ($(subst /,_,$(shell uname -s)),OS2)
CSRCS		+= su_unix.c
else
CSRCS		+= os2updt.c su_wjava.c
CPPSRCS		+= su_win.cpp su_mocha.cpp nsFolderSpec.cpp nsUninstallObject.cpp nsVersionInfo.cpp nsInstallDelete.cpp nsInstallExecute.cpp nsWinProfile.cpp nsInstallFile.cpp nsWinProfileItem.cpp nsInstallPatch.cpp nsWinReg.cpp nsSUError.cpp nsWinRegItem.cpp nsSoftwareUpdate.cpp nsWinRegValue.cpp

endif

EXPORTS		= ../include/softupdt.h \
		  $(NULL)

include $(DEPTH)/config/rules.mk

ifeq ($(OS_ARCH),OS2)
INCLUDES	+= -I$(DEPTH)/cmd/os2fe/nfc/include -I$(DEPTH)/cmd/os2fe
endif
