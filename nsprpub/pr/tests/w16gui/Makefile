#
# The contents of this file are subject to the Netscape Public License
# Version 1.1 (the "NPL"); you may not use this file except in
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

#! gmake



MOD_DEPTH = ../../..

include $(MOD_DEPTH)/config/config.mk

INCLUDES = -I$(DIST)/include
W16STDIO = $(MOD_DEPTH)/pr/src/md/windows/$(OBJDIR)/w16stdio.$(OBJ_SUFFIX)

CSRCS = poppad.c \
       popfile.c \
       popfont.c \
       popfind.c \
       popprnt0.c
          

INCLUDES = -I$(DIST)/include/private -I$(DIST)/include 
LIBPR = $(DIST)/lib/nspr$(MOD_VERSION).lib
LIBPLDS = $(DIST)/lib/plds$(MOD_VERSION).lib
TARGETS = $(OBJDIR)/poppad.exe
OS_CFLAGS = $(OS_EXE_CFLAGS)

include $(MOD_DEPTH)/config/rules.mk


ifeq ($(OS_TARGET),WIN16)    
$(OBJDIR)/poppad.exe: $(OBJS)
	@$(MAKE_OBJDIR)
	echo system windows >w16link
	echo name $@  >>w16link
	echo option map >>w16link
	echo option stack=16K >>w16link
	echo debug $(DEBUGTYPE) all >>w16link
	echo file >>w16link
	echo $(OBJDIR)\\poppad.$(OBJ_SUFFIX), >>w16link
	echo $(OBJDIR)\\popfile.$(OBJ_SUFFIX), >>w16link
	echo $(OBJDIR)\\popfont.$(OBJ_SUFFIX), >>w16link
	echo $(OBJDIR)\\popfind.$(OBJ_SUFFIX), >>w16link
	echo $(OBJDIR)\\popprnt0.$(OBJ_SUFFIX), >>w16link
	echo $(W16STDIO) >>w16link
	echo library $(LIBPR)      >>w16link
	echo library $(LIBPLDS)	   >>w16link
	echo library clibl, commdlg >>w16link
	echo library winsock.lib   >>w16link
	wlink @w16link.
	wrc -bt=windows poppad.rc $(OBJDIR)\\poppad.exe
else    
$(OBJDIR)/poppad.exe: $(OBJS)
	link $(LDOPTS) $< $(LIBPLC) $(LIBPR) wsock32.lib -out:$@
endif

export:: $(TARGETS)

install:: export

clean::
	rm -rf $(TARGETS)
