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

ifeq ($(OS_TARGET), WIN16)
OS_CFLAGS = $(OS_EXE_CFLAGS)
W16STDIO = $(MOD_DEPTH)/pr/src/md/windows/$(OBJDIR)/w16stdio.$(OBJ_SUFFIX)
endif

ifeq ($(OS_TARGET), OS2)
OS_CFLAGS = $(OS_EXE_CFLAGS)
endif

CSRCS = gc1.c thrashgc.c

ifeq ($(OS_ARCH), WINNT)
PROG_SUFFIX = .exe
else
PROG_SUFFIX =
endif

PROGS = $(addprefix $(OBJDIR)/, $(CSRCS:.c=$(PROG_SUFFIX)))

TARGETS = $(PROGS) $(OBJS)

INCLUDES = -I$(DIST)/include

# Setting the variables LDOPTS and LIBPR.  We first initialize
# them to the default values, then adjust them for some platforms.
LDOPTS = -L$(DIST)/lib
NSPR_VERSION = $(MOD_VERSION)
GC_VERSION = $(MOD_VERSION)
LIBPR = -lnspr$(NSPR_VERSION)
LIBPLC = -lplc$(NSPR_VERSION)
LIBGC = -lmsgc$(GC_VERSION)

ifeq ($(OS_ARCH), WINNT)
ifeq ($(OS_TARGET), WIN16)
  LIBPR = $(DIST)/lib/nspr$(NSPR_VERSION).lib
  LIBPLC = $(DIST)/lib/plc$(NSPR_VERSION).lib
  LIBGC= $(DIST)/lib/msgc$(GC_VERSION).lib
else
ifeq ($(OS_TARGET),OS2)
  LDOPTS = -NOE -DEBUG -nologo -PMTYPE:VIO
  LIBPR = $(DIST)/lib/nspr$(NSPR_VERSION).lib
  LIBPLC = $(DIST)/lib/plc$(NSPR_VERSION).lib
  LIBGC= $(DIST)/lib/msgc$(GC_VERSION).lib
else
  LDOPTS = -NOLOGO -DEBUG -DEBUGTYPE:CV -INCREMENTAL:NO
  LIBPR = $(DIST)/lib/libnspr$(NSPR_VERSION).$(LIB_SUFFIX)
  LIBPLC = $(DIST)/lib/libplc$(NSPR_VERSION).$(LIB_SUFFIX)
  LIBGC= $(DIST)/lib/libmsgc$(GC_VERSION).$(LIB_SUFFIX)
endif
endif
endif

ifneq ($(OS_ARCH), WINNT)
PWD = $(shell pwd)
endif

ifeq ($(OS_ARCH), IRIX)
LDOPTS += -rpath $(PWD)/$(DIST)/lib -rdata_shared

# For 6.x machines, include this flag
ifeq ($(basename $(OS_RELEASE)),6)
ifeq ($(USE_N32),1)
LDOPTS += -n32
else
LDOPTS += -32
endif
endif

endif

ifeq ($(OS_ARCH), OSF1)
# I haven't figured out how to pass -rpath to cc on OSF1 V3.2, so
# we do static linking.
ifeq ($(OS_RELEASE), V3.2)
  LIBPR = $(DIST)/lib/libnspr$(NSPR_VERSION).a
  LIBPLC = $(DIST)/lib/libplc$(NSPR_VERSION).a
  LIBGC = $(DIST)/lib/libmsgc$(GC_VERSION).a
  EXTRA_LIBS = -lc_r
else
  LDOPTS += -rpath $(PWD)/$(DIST)/lib
endif
endif

ifeq ($(OS_ARCH), HP-UX)
LDOPTS += -z -Wl,+s,+b,$(PWD)/$(DIST)/lib
endif

# AIX
ifeq ($(OS_ARCH),AIX)
LDOPTS += -blibpath:$(PWD)/$(DIST)/lib:/usr/lib:/lib
ifeq ($(OS_ARCH)$(OS_RELEASE),AIX4.1)
LIBPR = -lnspr$(NSPR_VERSION)_shr
LIBPLC = -lplc$(NSPR_VERSION)_shr
LIBGC = -lmsgc$(GC_VERSION)_shr
else
LDOPTS += -brtl
EXTRA_LIBS = -ldl
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

ifeq ($(OS_ARCH),NEC)
EXTRA_LIBS = $(OS_LIBS)
# This hardcodes in the executable programs the directory to find
# libnspr.so etc. at program startup.  Equivalent to the -R or -rpath
# option for ld on other platforms.
export LD_RUN_PATH = $(PWD)/$(DIST)/lib
endif

ifeq ($(OS_ARCH), NCR)
# XXX: We see some strange problems when we link with libnspr.so.
# So for now we use static libraries on NCR.  The shared library
# stuff below is commented out.
LIBPR = $(DIST)/lib/libnspr$(NSPR_VERSION).a
LIBPLC = $(DIST)/lib/libplc$(NSPR_VERSION).a
LIBGC = $(DIST)/lib/libmsgc$(GC_VERSION).a
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

ifeq ($(OS_ARCH), Linux)
ifeq ($(OS_RELEASE), 1.2)
EXTRA_LIBS = -ldl
endif
endif

ifeq ($(OS_ARCH), SCOOS)
# SCO Unix needs to link against -lsocket again even though we
# already linked with these system libraries when we built libnspr.so.
EXTRA_LIBS = -lsocket
# This hardcodes in the executable programs the directory to find
# libnspr.so etc. at program startup.  Equivalent to the -R or -rpath 
# option for ld on other platforms.
export LD_RUN_PATH = $(PWD)/$(DIST)/lib
endif

ifeq ($(OS_ARCH),SINIX)
EXTRA_LIBS = -lsocket -lnsl -lresolv -ldl
# This hardcodes in the executable programs the directory to find
# libnspr.so etc. at program startup.  Equivalent to the -R or -rpath
# option for ld on other platforms.
export LD_RUN_PATH = $(PWD)/$(DIST)/lib
endif

ifeq ($(OS_ARCH), UNIXWARE)
export LD_RUN_PATH = $(PWD)/$(DIST)/lib
endif

ifeq ($(OS_ARCH),BSD_OS)
EXTRA_LIBS = -ldl
endif

ifeq ($(OS_ARCH),DGUX)
EXTRA_LIBS = -lsocket -lnsl -ldl
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
	$(CC) $(AIX_LINK_OPTS) -o $(AIX_TMP) $< $(DIST)/lib/libnspr$(NSPR_VERSION).a
	$(CC) -o $@ $(AIX_TMP) $(AIX_WRAP)
	rm -f $(AIX_TMP)

else

# All platforms that are not AIX pre-4.2.

$(OBJDIR)/%$(PROG_SUFFIX): $(OBJDIR)/%.$(OBJ_SUFFIX)
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH), WINNT)
ifeq ($(OS_TARGET),WIN16)
	echo system windows >w16link
	echo name $@  >>w16link
	echo option map >>w16link
#	echo option CASEEXACT >>w16link
	echo option stack=16K >>w16link
	echo debug $(DEBUGTYPE) all >>w16link
	echo file >>w16link
	echo $< ,  >>w16link
	echo $(W16STDIO) >>w16link
	echo library  >>w16link
	echo $(LIBPR),	     >>w16link
	echo $(LIBPLC),		>>w16link
	echo $(LIBGC),		 >>w16link
	echo winsock.lib     >>w16link
	wlink @w16link.
else
ifeq ($(OS_TARGET),OS2)
	$(LINK) $(LDOPTS) $< $(LIBGC) $(LIBPLC) $(LIBPR) so32dll.lib tcp32dll.lib -MAP:$(@:.exe=.map) -out:$@
else
	link $(LDOPTS) $< $(LIBGC) $(LIBPLC) $(LIBPR) wsock32.lib -out:$@
endif
endif
else
	$(CC) $(XCFLAGS) $< $(LDOPTS) $(LIBGC) $(LIBPLC) $(LIBPR) $(EXTRA_LIBS) -o $@
endif

endif

export:: $(TARGETS)
export:: install
clean::
	rm -f $(TARGETS)
