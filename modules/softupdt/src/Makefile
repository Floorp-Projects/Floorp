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
            jar security lay style libreg network progress zlib

ifeq ($(MOZ_OJI),1)
REQUIRES	+= oji npj
endif
ifeq ($(MOZ_JAVA),1)
REQUIRES	+= applet
endif

JRI_GEN		= netscape.softupdate.FolderSpec \
          netscape.softupdate.InstallDelete \
          netscape.softupdate.InstallObject \
		  netscape.softupdate.InstallFile \
		  netscape.softupdate.InstallPatch \
		  netscape.softupdate.InstallExecute \
		  netscape.softupdate.Registry \
		  netscape.softupdate.RegistryNode \
		  netscape.softupdate.RegistryException \
		  netscape.softupdate.RegKeyEnumerator \
		  netscape.softupdate.RegEntryEnumerator \
		  netscape.softupdate.SoftwareUpdate \
		  netscape.softupdate.SoftUpdateException \
		  netscape.softupdate.Strings \
		  netscape.softupdate.Trigger \
		  netscape.softupdate.VersionInfo \
		  netscape.softupdate.VersionRegistry \
		  netscape.softupdate.VerRegEnumerator \
		  java.lang.Integer \
		  $(NULL)

CSRCS		= softupdt.c \
		  su_trigger.c \
		  su_patch.c \
		  su_folderspec.c \
		  su_instl.c \
		  su_nodl.c \
		  vr_java.c \
		  $(NULL)

ifneq ($(subst /,_,$(shell uname -s)),OS2)
CSRCS		+= su_unix.c
else
CSRCS		+= os2updt.c su_wjava.c
CPPSRCS		+= su_win.cpp
JRI_GEN		+= netscape.softupdate.WinReg netscape.softupdate.WinProfile netscape.softupdate.WinRegValue
endif

EXPORTS		= ../include/softupdt.h \
          $(JRI_GEN_DIR)/netscape_softupdate_InstallDelete.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_InstallFile.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_InstallObject.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_SoftwareUpdate.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_SoftUpdateException.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_VersionInfo.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_VersionRegistry.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_VerRegEnumerator.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_Registry.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_RegistryNode.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_RegistryException.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_RegKeyEnumerator.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_RegEntryEnumerator.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_Strings.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_Trigger.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_FolderSpec.h \
		  $(NULL)

include $(DEPTH)/config/rules.mk

ifeq ($(OS_ARCH),OS2)
INCLUDES	+= -I$(DEPTH)/cmd/os2fe/nfc/include -I$(DEPTH)/cmd/os2fe
endif

$(OBJDIR)/su_folderspec.o: $(JRI_GEN_DIR)/netscape_softupdate_FolderSpec.c \
			   $(JRI_GEN_DIR)/netscape_softupdate_SoftwareUpdate.h

$(OBJDIR)/su_instl.o:	$(JRI_GEN_DIR)/netscape_softupdate_InstallFile.c \
            $(JRI_GEN_DIR)/netscape_softupdate_InstallDelete.c	 \
			$(JRI_GEN_DIR)/netscape_softupdate_InstallObject.c \
			$(JRI_GEN_DIR)/netscape_softupdate_InstallExecute.c \
			$(JRI_GEN_DIR)/netscape_softupdate_InstallPatch.c \
			$(JRI_GEN_DIR)/netscape_softupdate_SoftwareUpdate.h \
			$(JRI_GEN_DIR)/netscape_softupdate_SoftUpdateException.h \
			$(JRI_GEN_DIR)/netscape_softupdate_Strings.h

$(OBJDIR)/su_trigger.o:	$(JRI_GEN_DIR)/netscape_softupdate_Trigger.c \
			$(JRI_GEN_DIR)/netscape_softupdate_SoftUpdateException.c \
			$(JRI_GEN_DIR)/netscape_softupdate_SoftwareUpdate.c \
			$(JRI_GEN_DIR)/netscape_softupdate_Strings.c

$(OBJDIR)/vr_java.o: $(JRI_GEN_DIR)/netscape_softupdate_VersionInfo.h \
          $(JRI_GEN_DIR)/netscape_softupdate_VersionRegistry.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_VerRegEnumerator.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_Registry.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_RegistryNode.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_RegistryException.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_RegKeyEnumerator.h \
		  $(JRI_GEN_DIR)/netscape_softupdate_RegEntryEnumerator.h
