#! gmake

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

MOD_DEPTH	= ..

include $(MOD_DEPTH)/config/config.mk

CSRCS	= nsinstall.c pathsub.c

PLSRCS	= nfspwd.pl

ifneq ($(OS_ARCH),WINNT)
PROGRAM	= $(OBJDIR)/nsinstall
TARGETS = $(PROGRAM) $(PLSRCS:.pl=)
endif

include $(MOD_DEPTH)/config/rules.mk

# Redefine MAKE_OBJDIR for just this directory
define MAKE_OBJDIR
if test ! -d $(@D); then rm -rf $(@D); mkdir $(@D); fi
endef

export:: $(TARGETS)
