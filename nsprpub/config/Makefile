#! gmake

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

MOD_DEPTH	= ..

include $(MOD_DEPTH)/config/config.mk

CSRCS	= nsinstall.c now.c

PLSRCS	= nfspwd.pl

ifeq ($(OS_ARCH), WINNT)
PROG_SUFFIX = .exe
else
PROG_SUFFIX =
endif

# Temporary workaround to disable the generation of
# library build time because now.c uses the 'long long'
# data type that's not available on some platforms.
ifeq (,$(filter-out NEC NEXTSTEP QNX SCOOS UNIXWARE,$(OS_ARCH)))
DEFINES += -DOMIT_LIB_BUILD_TIME
endif

ifeq ($(OS_ARCH), IRIX)
    ifeq ($(basename $(OS_RELEASE)),6)
	    ifeq ($(USE_N32),1)
		    XLDOPTS += -n32 -Wl,-woff,85
		    ifeq ($(OS_RELEASE), 6_2)
			    XLDOPTS += -Wl,-woff,85
		    endif
	    else
		    ifeq ($(USE_64),1)
			    XLDOPTS += -64
		    else
			    XLDOPTS += -32
		    endif
	    endif
    endif
endif

ifeq ($(OS_ARCH), HP-UX)
    ifeq ($(USE_64),1)
        XLDOPTS += +DD64
    endif
endif

ifdef XP_OS2_EMX
XCFLAGS = $(OS_EXE_CFLAGS)
XLDOPTS = -Zlinker /PM:VIO
endif

ifeq ($(MOZ_OS2_TOOLS),VACPP)
OS_CFLAGS = $(OS_EXE_CFLAGS)
endif

include $(MOD_DEPTH)/config/rules.mk

PROGS	= $(OBJDIR)/now$(PROG_SUFFIX)

ifeq ($(OS_ARCH),WINNT)
TARGETS = $(PROGS)
else
PROGS	+= $(OBJDIR)/nsinstall$(PROG_SUFFIX)
TARGETS = $(PROGS) $(PLSRCS:.pl=)
endif

# Redefine MAKE_OBJDIR for just this directory
define MAKE_OBJDIR
if test ! -d $(@D); then rm -rf $(@D); mkdir $(@D); else true; fi
endef

export:: $(TARGETS)

$(OBJDIR)/%$(PROG_SUFFIX): $(OBJDIR)/%.$(OBJ_SUFFIX)
	@$(MAKE_OBJDIR)
ifeq ($(MOZ_OS2_TOOLS),VACPP)
	$(LINK) $(EXEFLAGS) $<
else
	$(CC) $(XCFLAGS) $< $(XLDOPTS) -o $@
endif
