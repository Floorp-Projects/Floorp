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
endif

DEPTH	= ../../..

MODULE	= libfont

JMC_GEN	= Mnfdoer \
	  Mnff \
	  Mnffbc \
	  Mnffbp \
	  Mnffbu \
	  Mnffmi \
	  Mnffp \
	  Mnfrc \
	  Mnfrf \
	  Mnfstrm \
	  Mnfdlm \
	  $(NULL)

EXPORTS	= $(JMC_GEN_DIR)/Mnfdoer.h \
	  $(JMC_GEN_DIR)/Mnff.h \
	  $(JMC_GEN_DIR)/Mnffbc.h \
	  $(JMC_GEN_DIR)/Mnffbp.h \
	  $(JMC_GEN_DIR)/Mnffbu.h \
	  $(JMC_GEN_DIR)/Mnffmi.h \
	  $(JMC_GEN_DIR)/Mnffp.h \
	  $(JMC_GEN_DIR)/Mnfrc.h \
	  $(JMC_GEN_DIR)/Mnfrf.h \
	  $(JMC_GEN_DIR)/Mnfstrm.h \
	  $(JMC_GEN_DIR)/Mnfdlm.h \
	  $(NULL)

include $(DEPTH)/config/rules.mk
