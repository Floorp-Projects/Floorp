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

ifndef MOZ_JAVA
LOCAL_JMC_SUBDIR =  ../jmcgen/include
LOCAL_JMC_SRC_SUBDIR =  ../jmcgen/src
endif

DEPTH		= ../../..

MODULE		= libfont
LIBRARY_NAME	= font

CSRCS		= libfont.c

CPPSRCS		= cdoer.cpp \
		  cf.cpp \
		  cfmi.cpp \
		  cfb.cpp \
		  crc.cpp \
		  f.cpp \
		  fmi.cpp \
		  fb.cpp \
		  rc.cpp \
		  wfSzList.cpp \
		  wffpPeer.cpp \
		  wffpCat.cpp \
		  wfFCache.cpp \
		  wfList.cpp \
		  wfMime.cpp \
		  wfStream.cpp \
		  wfMisc.cpp \
		  wfDlm.cpp \
		  $(NULL)

EXPORTS		= nf.h coremem.h

REQUIRES	= libfont java jtools dbm nspr img layer util pref js htmldlgs security

JMC_GEN		= Mcf Mcfb Mcfmi Mcrc Mcdoer
JMC_GEN_FLAGS	= -cimpl

include $(DEPTH)/config/rules.mk

INCLUDES	+= -I.

SDK_TARGET	= producer-sdk
SDK_NAME	= FontProducer-sdk-v1.0
SDK_DIR		= $(OBJDIR)/$(SDK_TARGET)

ifndef MOZ_JAVA
export:: 
	@echo "Copying JMC src files from $(LOCAL_JMC_SRC_SUBDIR)/*.c to cwd"
	@for FILE in $(LOCAL_JMC_SRC_SUBDIR)/*.c ;\
	 do \
	   $(NSINSTALL) -t $${FILE} . ;\
	 done
endif

ifneq ($(OS_ARCH),OS2)
export:: 
	@echo "Fixing jmc generated header file : $(JMC_GEN_DIR)/Mcfb.h"
	@ls -l $(JMC_GEN_DIR)/Mcfb.h
	@if fgrep cfbImpl2Object\( $(JMC_GEN_DIR)/Mcfb.h ;\
	then \
		echo "/cfbImpl2Object(\n.s/cfbImpl2Object(/cfbImpl2Object_1(/\n1\n/Object2cfbImpl(\n.s/Object2cfbImpl(/Object2cfbImpl_1(/\nw\nq\n" | (ed $(JMC_GEN_DIR)/Mcfb.h; echo) ;\
		echo "/cfbImpl2Object(\n.s/cfbImpl2Object(/cfbImpl2Object_2(/\n1\n/Object2cfbImpl(\n.s/Object2cfbImpl(/Object2cfbImpl_2(/\nw\nq\n" | (ed $(JMC_GEN_DIR)/Mcfb.h; echo) ;\
		echo "Fixed : $(JMC_GEN_DIR)/Mcfb.h" ;\
	else \
		echo "No fix required. Skipping..." ;\
	fi
endif

$(SDK_TARGET):: $(PRODUCER_SDK_FILES)
	echo "Generating producer sdk..."
	$(NSINSTALL) -m 0444 $(PUBLIC)/libfont/* $(SDK_DIR)/$(SDK_NAME)/include
	$(NSINSTALL) -m 0444 Pcfp.h $(JMC_GEN_DIR)/Mcfp.h $(SDK_DIR)/$(SDK_NAME)/include
	$(NSINSTALL) -m 0444 Pcrf.h $(JMC_GEN_DIR)/Mcrf.h $(SDK_DIR)/$(SDK_NAME)/include
	$(NSINSTALL) -m 0444 $(DEPTH)/include/xp_mem.h $(SDK_DIR)/$(SDK_NAME)/include
	$(NSINSTALL) -m 0444 $(DEPTH)/include/xp_core.h $(SDK_DIR)/$(SDK_NAME)/include
	$(NSINSTALL) -m 0444 $(PUBLIC)/jtools/jmc.h $(SDK_DIR)/$(SDK_NAME)/include
	$(NSINSTALL) -m 0444 $(PUBLIC)/java/jritypes.h $(SDK_DIR)/$(SDK_NAME)/include
	$(NSINSTALL) -m 0444 $(PUBLIC)/java/jri_md.h $(SDK_DIR)/$(SDK_NAME)/include
	$(NSINSTALL) -m 0444 $(PUBLIC)/java/jri.h $(SDK_DIR)/$(SDK_NAME)/include
	$(NSINSTALL) -m 0444 $(PUBLIC)/nspr/prtypes.h $(SDK_DIR)/$(SDK_NAME)/include
	$(NSINSTALL) -m 0444 $(PUBLIC)/nspr/prcpucfg.h $(SDK_DIR)/$(SDK_NAME)/include
	$(NSINSTALL) -m 0664 cfp.c crf.c $(SDK_DIR)/$(SDK_NAME)/src
	$(NSINSTALL) -m 0444 $(JMC_GEN_DIR)/Mcfp.c $(JMC_GEN_DIR)/Mcrf.c $(SDK_DIR)/$(SDK_NAME)/src
	$(NSINSTALL) -m 0444 ../sdk/unix/makefile.sgi $(SDK_DIR)/$(SDK_NAME)/unix
	(cd $(SDK_DIR); tar cvf - $(SDK_NAME)) | gzip -cf > $(OBJDIR)/$(SDK_NAME).tar.gz; rm -rf $(SDK_DIR)
