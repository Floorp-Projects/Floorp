# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape Portable Runtime (NSPR).
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
# 

#! gmake

MOD_DEPTH = ../..

include $(MOD_DEPTH)/config/config.mk

ifeq ($(OS_ARCH), WINNT)
# DIRS = windows
endif

ifeq ($(OS_TARGET), WIN16)
OS_CFLAGS = $(OS_EXE_CFLAGS)
endif

CSRCS = \
	arena.c \
	string.c \
	base64t.c

ifeq (,$(filter-out WINNT OS2,$(OS_ARCH)))
CSRCS += arena.c
endif

ifeq (,$(filter-out WINNT OS2,$(OS_ARCH)))
PROG_SUFFIX = .exe
else
PROG_SUFFIX =
endif

PROGS = $(addprefix $(OBJDIR)/, $(CSRCS:.c=$(PROG_SUFFIX)))

TARGETS = $(PROGS) $(OBJS)

INCLUDES = -I$(includedir)

# Setting the variables LDOPTS and LIBPR.  We first initialize
# them to the default values, then adjust them for some platforms.
LDOPTS = -L$(libdir)
LIBPR = -lnspr$(MOD_VERSION)
LIBPLC = -lplc$(MOD_VERSION)
LIBPLDS = -lplds$(MOD_VERSION)

ifeq ($(OS_ARCH), WINNT)
ifeq ($(OS_TARGET), WIN16)
  LIBPR = $(libdir)/nspr$(MOD_VERSION).lib
  LIBPLC= $(libdir)/plc$(MOD_VERSION).lib
  LIBPLDS= $(libdir)/plds$(MOD_VERSION).lib
else
  LDOPTS = -NOLOGO -DEBUG -DEBUGTYPE:CV -INCREMENTAL:NO
  ifeq ($(OS_TARGET), WIN95)
  LIBPR = $(libdir)/nspr$(MOD_VERSION).$(LIB_SUFFIX)
  LIBPLC= $(libdir)/plc$(MOD_VERSION).$(LIB_SUFFIX)
  LIBPLDS= $(libdir)/plds$(MOD_VERSION).$(LIB_SUFFIX)
  else
  LIBPR = $(libdir)/libnspr$(MOD_VERSION).$(LIB_SUFFIX)
  LIBPLC= $(libdir)/libplc$(MOD_VERSION).$(LIB_SUFFIX)
  LIBPLDS= $(libdir)/libplds$(MOD_VERSION).$(LIB_SUFFIX)
  endif
endif
endif

ifeq ($(OS_ARCH),OS2)
  ifeq ($(MOZ_OS2_TOOLS),VACPP)
    LDOPTS = -NOE -DEBUG -nologo -PMTYPE:VIO /S:32768
    LIBPR = $(libdir)/nspr$(MOD_VERSION).lib
    LIBPLC = $(libdir)/plc$(MOD_VERSION).lib
    LIBPLDS= $(libdir)/plds$(MOD_VERSION).$(LIB_SUFFIX)	
  else
    LDOPTS += -Zomf -Zlinker /PM:VIO
  endif
endif

ifneq ($(OS_ARCH), WINNT)
PWD = $(shell pwd)
endif

ifeq ($(OS_ARCH), IRIX)
LDOPTS += -rpath $(PWD)/$(libdir)
endif

ifeq ($(OS_ARCH), Linux)
    ifeq ($(OS_RELEASE), 1.2)
        EXTRA_LIBS = -ldl
    else
        LDOPTS += -Xlinker -rpath $(PWD)/$(libdir)
        ifeq ($(USE_PTHREADS),1)
            EXTRA_LIBS = -lpthread
        endif
    endif
endif

ifeq ($(OS_ARCH), OSF1)
LDOPTS += -rpath $(PWD)/$(libdir) -lpthread
endif

ifeq ($(OS_ARCH), HP-UX)
LDOPTS += -Wl,+s,+b,$(PWD)/$(libdir)
endif

# AIX
ifeq ($(OS_ARCH),AIX)
LDOPTS += -blibpath:$(PWD)/$(libdir):/usr/lib:/lib
LIBPR = -lnspr$(MOD_VERSION)_shr
LIBPLC = -lplc$(MOD_VERSION)_shr
endif

# Solaris
ifeq ($(OS_ARCH), SunOS)
ifneq ($(OS_RELEASE), 4.1.3_U1)
ifdef NS_USE_GCC
LDOPTS += -Xlinker -R -Xlinker $(PWD)/$(libdir)
else
LDOPTS += -R $(PWD)/$(libdir)
endif
endif

# SunOS 5.4 and 5.5 need to link with -lthread or -lpthread,
# even though we already linked with these system libraries
# when we built libnspr.so.
ifeq ($(OS_RELEASE), 5.4)
EXTRA_LIBS = -lthread
endif

ifeq ($(OS_RELEASE), 5.5)
ifdef USE_PTHREADS
EXTRA_LIBS = -lpthread
else
EXTRA_LIBS = -lthread
endif
endif
endif # SunOS

ifeq ($(OS_ARCH), NCR)
# NCR needs to link against -lsocket -lnsl (and -lc, which is linked
# implicitly by $(CC)) again even though we already linked with these
# system libraries when we built libnspr.so.
EXTRA_LIBS = -lsocket -lnsl
# This hardcodes in the executable programs the directory to find
# libnspr.so etc. at program startup.  Equivalent to the -R or -rpath
# option for ld on other platforms.
export LD_RUN_PATH = $(PWD)/$(libdir)
endif

#####################################################
#
# The rules
#
#####################################################

include $(MOD_DEPTH)/config/rules.mk

AIX_PRE_4_2 = 0
ifeq ($(OS_ARCH),AIX)
ifneq ($(OS_RELEASE),4.2)
ifneq ($(USE_PTHREADS), 1)
#AIX_PRE_4_2 = 1
endif
endif
endif

ifeq ($(AIX_PRE_4_2),1)

# AIX releases prior to 4.2 need a special two-step linking hack
# in order to both override the system select() and be able to 
# get at the original system select().
#
# We use a pattern rule in ns/nspr20/config/rules.mk to generate
# the .$(OBJ_SUFFIX) file from the .c source file, then do the
# two-step linking hack below.

$(OBJDIR)/%: $(OBJDIR)/%.$(OBJ_SUFFIX)
	@$(MAKE_OBJDIR)
	rm -f $@ $(AIX_TMP)
	$(CC) $(AIX_LINK_OPTS) -o $(AIX_TMP) $< $(libdir)/libnspr$(MOD_VERSION).a
	$(CC) -o $@ $(AIX_TMP) $(AIX_WRAP)
	rm -f $(AIX_TMP)

else

# All platforms that are not AIX pre-4.2.

$(OBJDIR)/%$(PROG_SUFFIX): $(OBJDIR)/%.$(OBJ_SUFFIX)
	@$(MAKE_OBJDIR)

ifeq ($(OS_ARCH), WINNT)
ifeq ($(OS_TARGET),WIN16)
	echo system windows >w16link
	echo option map >>w16link
	echo option stack=10K >>w16link
	echo option heapsize=32K >>w16link
	echo debug $(DEBUGTYPE) all >>w16link
	echo name $@  >>w16link
	echo file >>w16link
	echo $<  >>w16link
	echo library  >>w16link
	echo $(LIBPR),	     >>w16link
	echo $(LIBPLC),		 >>w16link
	echo winsock.lib     >>w16link
	wlink @w16link.
else
	link $(LDOPTS) $< $(LIBPLC) $(LIBPLDS) $(LIBPR) wsock32.lib -out:$@
endif
else
ifeq ($(OS_ARCH),OS2)
	$(LINK) $(EXEFLAGS) $(LDOPTS) $< $(LIBPLC)  $(LIBPLDS) $(LIBPR) $(OS_LIBS) $(EXTRA_LIBS)
else
	$(CC) $(XCFLAGS) $< $(LDOPTS) $(LIBPLC) $(LIBPLDS) $(LIBPR) $(EXTRA_LIBS) -o $@
endif
endif
endif

export:: $(TARGETS)
clean::
	rm -f $(TARGETS)



