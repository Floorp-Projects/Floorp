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

MOD_DEPTH = ../../../..

include $(MOD_DEPTH)/config/config.mk

ifeq ($(OS_TARGET), WIN16)
OS_CFLAGS = $(OS_EXE_CFLAGS)
endif

CXXSRCS =           \
	ranfile.cpp     \
	thread.cpp      \
	interval.cpp    \
	time.cpp        \
	fileio.cpp      \
	switch.cpp      \
	tpd.cpp         \
	$(NULL)

OBJS = $(addprefix $(OBJDIR)/,$(CXXSRCS:.cpp=.$(OBJ_SUFFIX)))

ifeq ($(OS_ARCH), WINNT)
PROG_SUFFIX = .exe
else
PROG_SUFFIX =
endif

PROGS = $(addprefix $(OBJDIR)/, $(CXXSRCS:.cpp=$(PROG_SUFFIX)))

TARGETS = $(PROGS) $(OBJS)

INCLUDES = -I.. -I$(DIST)/include

# Setting the variables LDOPTS and LIBPR.  We first initialize
# them to the default values, then adjust them for some platforms.
LDOPTS = -L$(DIST)/lib
LIBPR = -lnspr$(MOD_VERSION)
LIBPL = -lplc$(MOD_VERSION)

ifeq ($(OS_ARCH), IRIX)
    LDOPTS += -rpath $(PWD)/$(DIST)/lib -rdata_shared
    # For 6.x machines, include this flag
    ifeq ($(basename $(OS_RELEASE)),6)
        ifeq ($(USE_N32),1)
            LDOPTS += -n32
        else
            LDOPTS += -32
        endif

        ifeq ($(USE_PTHREADS), 1)
            ifeq ($(OS_RELEASE), 6.2)
                LDOPTS += -Wl,-woff,85
            endif
        endif
    endif
endif

# Solaris
ifeq ($(OS_ARCH), SunOS)
    ifneq ($(OS_RELEASE), 4.1.3_U1)
        ifdef NS_USE_GCC
            LDOPTS += -Xlinker -R -Xlinker $(PWD)/$(DIST)/lib
        else
            LDOPTS += -R $(PWD)/$(DIST)/lib
        endif
    endif

    ifneq ($(LOCAL_THREADS_ONLY),1)
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
    endif # LOCAL_THREADS_ONLY
endif # SunOS

ifeq ($(OS_ARCH), WINNT)
ifeq ($(OS_TARGET), WIN16)
  LIBPR = $(DIST)/lib/nspr$(MOD_VERSION).lib
  LIBPL = $(DIST)/lib/plc$(MOD_VERSION).lib
else
  LDOPTS = -NOLOGO -DEBUG -DEBUGTYPE:CV -INCREMENTAL:NO
  LIBPR = $(DIST)/lib/libnspr$(MOD_VERSION).$(LIB_SUFFIX)
  LIBPL = $(DIST)/lib/libplc$(MOD_VERSION).$(LIB_SUFFIX)
endif
endif

ifneq ($(OS_ARCH), WINNT)
PWD = $(shell pwd)
endif

ifeq ($(OS_ARCH), OSF1)
LDOPTS += -rpath $(PWD)/$(DIST)/lib
endif

ifeq ($(OS_ARCH), HP-UX)
    LDOPTS += -Wl,+s,+b,$(PWD)/$(DIST)/lib
endif

# AIX
ifeq ($(OS_ARCH),AIX)
    LDOPTS += -blibpath:$(PWD)/$(DIST)/lib:/usr/lib:/lib
    ifeq ($(OS_ARCH)$(OS_RELEASE),AIX4.1)
        LIBPR = -lnspr$(MOD_VERSION)_shr
        LIBPLC = -lplc$(MOD_VERSION)_shr
    else
        LDOPTS += -brtl
        EXTRA_LIBS = -ldl
    endif
endif

ifeq ($(OS_ARCH), Linux)
    ifeq ($(OS_RELEASE), 1.2)
        EXTRA_LIBS = -ldl
    else
        LDOPTS += -Xlinker -rpath $(PWD)/$(DIST)/lib
        ifeq ($(USE_PTHREADS),1)
            EXTRA_LIBS = -lpthread
        endif
    endif
endif

ifeq ($(OS_ARCH), NCR)
# XXX: We see some strange problems when we link with libnspr.so.
# So for now we use static libraries on NCR.  The shared library
# stuff below is commented out.
LIBPR = $(DIST)/lib/libnspr$(MOD_VERSION).a
LIBPL = $(DIST)/lib/libplc$(MOD_VERSION).a
EXTRA_LIBS = -lsocket -lnsl -ldl

# NCR needs to link against -lsocket -lnsl (and -lc, which is linked
# implicitly by $(CC)) again even though we already linked with these
# system libraries when we built libnspr.so.
#EXTRA_LIBS = -lsocket -lnsl
# This hardcodes in the executable programs the directory to find
# libnspr.so etc. at program startup.  Equivalent to the -R or -rpath 
# option for ld on other platforms.
#export LD_RUN_PATH = $(PWD)/$(DIST)/lib
endif

ifeq ($(OS_ARCH), SCO_SV)
# SCO Unix needs to link against -lsocket again even though we
# already linked with these system libraries when we built libnspr.so.
EXTRA_LIBS = -lsocket
# This hardcodes in the executable programs the directory to find
# libnspr.so etc. at program startup.  Equivalent to the -R or -rpath
# option for ld on other platforms.
export LD_RUN_PATH = $(PWD)/$(DIST)/lib
endif

ifeq ($(OS_ARCH), UNIXWARE)
export LD_RUN_PATH = $(PWD)/$(DIST)/lib
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
	$(CC) $(AIX_LINK_OPTS) -o $(AIX_TMP) $< $(DIST)/lib/libnspr$(MOD_VERSION).a
	$(CC) -o $@ $(AIX_TMP) $(AIX_WRAP)
	rm -f $(AIX_TMP)

else

# All platforms that are not AIX pre-4.2.

$(OBJDIR)/%$(PROG_SUFFIX): $(OBJDIR)/%.$(OBJ_SUFFIX)
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH), WINNT)
ifeq ($(OS_TARGET),WIN16)
	echo system windows         >w16link
	echo option map             >>w16link
	echo option stack=10K       >>w16link
	echo option heapsize=32K    >>w16link
	echo debug $(DEBUGTYPE) all >>w16link
	echo name $@                >>w16link
	echo file                   >>w16link
	echo $<                     >>w16link
	echo library                >>w16link
	echo $(LIBPR),	            >>w16link
	echo $(LIBPL),	            >>w16link
	echo winsock.lib            >>w16link
	wlink @w16link.
else
	link $(LDOPTS) $< $(LIBPR) $(LIBPL) wsock32.lib -out:$@
endif
else
	$(CCC) $(XCFLAGS) $< $(LDOPTS) $(LIBPR) $(LIBPL) $(EXTRA_LIBS) -o $@
endif

endif

export:: $(TARGETS)
install:: export
clean::
	rm -f $(TARGETS)

