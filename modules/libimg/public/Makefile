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
#

ifndef MOZ_JAVA
LOCAL_JMC_SUBDIR = .
endif


DEPTH		= ../../..

MODULE		= img

ifndef STANDALONE_IMAGE_LIB
JMC_GEN		= MIMGCBIF MIMGCB MPSIMGCB
JMC_GEN_FLAGS	= -cimpl -noexc -interface
endif

ifdef STANDALONE_IMAGE_LIB
EXPORTS		= il_types.h ni_pixmp.h libimg.h il_icons.h il_strm.h il_util.h \
    ilINetContext.h ilIURL.h ilINetReader.h ilIImageRenderer.h \
    ilISystemServices.h dummy_nc.h
else
EXPORTS		= il_types.h \
		  ni_pixmp.h \
		  libimg.h \
		  il_icons.h \
		  il_strm.h \
		  il_util.h \
		  ilIImageRenderer.h \
		  ilINetContext.h \
		  ilINetReader.h \
		  ilIURL.h \
		  dummy_nc.h \
		  $(JMC_GEN_DIR)/MIMGCBIF.h \
		  $(JMC_GEN_DIR)/MIMGCB.h \
		  $(JMC_GEN_DIR)/MPSIMGCB.h \
		  $(NULL)
endif

include $(DEPTH)/config/rules.mk
