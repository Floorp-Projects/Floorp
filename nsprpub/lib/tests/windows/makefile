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

CSRCS = winevent.c

ifeq ($(OS_ARCH), WINNT)
ifeq ($(OS_TARGET), WIN16)
  LIBPR = $(DIST)/lib/nspr$(MOD_VERSION).lib
  LIBPLC= $(DIST)/lib/plc$(MOD_VERSION).lib
  LIBPLDS= $(DIST)/lib/plds$(MOD_VERSION).lib
else
  LDOPTS = -NOLOGO -DEBUG -DEBUGTYPE:CV -INCREMENTAL:NO
  ifeq ($(OS_TARGET), WIN95)
  LIBPR = $(DIST)/lib/nspr$(MOD_VERSION).$(LIB_SUFFIX)
  LIBPLC= $(DIST)/lib/plc$(MOD_VERSION).$(LIB_SUFFIX)
  LIBPLDS= $(DIST)/lib/plds$(MOD_VERSION).lib
  else
  LIBPR = $(DIST)/lib/libnspr$(MOD_VERSION).$(LIB_SUFFIX)
  LIBPLC= $(DIST)/lib/libplc$(MOD_VERSION).$(LIB_SUFFIX)
  LIBPLDS= $(DIST)/lib/libplds$(MOD_VERSION).lib
  endif
endif
endif

TARGETS = $(OBJDIR)/winevent.exe
OS_CFLAGS = $(OS_EXE_CFLAGS)
LDOPTS = -NOLOGO -DEBUG -DEBUGTYPE:CV -INCREMENTAL:NO
LDFLAGS += -DEBUG
LIBPR += $(LIBPLDS)
LIBPR += kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib

include $(MOD_DEPTH)/config/rules.mk

$(OBJDIR)/winevent.exe: $(OBJS)
	link $(LDOPTS) $< $(LIBPLC) $(LIBPR) wsock32.lib -out:$@

export:: $(TARGETS)

install:: export

clean::
	rm -rf $(TARGETS)
