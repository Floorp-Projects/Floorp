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

#! gmake

MOD_DEPTH = ../..

include $(MOD_DEPTH)/config/config.mk

ifdef USE_PTHREADS
    DIRS = io linking malloc md memory misc pthreads threads
else
    DIRS = io linking malloc md memory misc threads
endif

#
# Define platform-dependent OS_LIBS
#

# Disable optimization of the nspr on SunOS4.1.3
ifeq ($(OS_ARCH),SunOS)
ifeq ($(OS_RELEASE),4.1.3_U1)
OPTIMIZER =
export OPTIMIZER
OS_LIBS			= -lm
else	# 4.1.3_U1
ifdef USE_PTHREADS
OS_LIBS			= -lpthread -lposix4 -lsocket -lnsl -ldl
else
ifdef LOCAL_THREADS_ONLY
OS_LIBS			= -lsocket -lnsl -ldl
else
OS_LIBS			= -lthread -lposix4 -lsocket -lnsl -ldl
endif	# LOCAL_THREADS_ONLY
endif	# USE_PTHREADS
endif	# 4.1.3_U1
endif	# SunOS

ifeq ($(OS_ARCH), IRIX)
ifeq ($(USE_PTHREADS), 1)
OS_LIBS += -lpthread
endif
endif

ifeq ($(OS_ARCH),AIX)
ifeq ($(CLASSIC_NSPR),1)
ifeq ($(OS_RELEASE),4.1)
OS_LIBS		+= -lsvld -lc
else
OS_LIBS		+= -ldl -lc
endif
else
ifeq ($(OS_RELEASE),4.1)
OS_LIBS		+= -lsvld -lC_r -lC -lpthreads -lc_r -lm /usr/lib/libc.a
else
OS_LIBS		+= -ldl -lC_r -lC -lpthreads -lc_r -lm /usr/lib/libc.a
endif
endif
endif

# On AIX, we override malloc in non-pthread versions.  On AIX 4.2 or
# above, this requires that we use the rtl-enabled version of libc.a.
ifeq ($(OS_ARCH),AIX)
ifneq ($(OS_RELEASE),4.1)
ifneq ($(USE_PTHREADS),1)
BUILD_AIX_RTL_LIBC = 1
AIX_RTL_LIBC	= $(OBJDIR)/libc.a
endif
endif
endif

ifeq ($(OS_ARCH),OSF1)
ifneq ($(OS_RELEASE),V2.0)
OS_LIBS		= -lc_r
endif
ifeq ($(USE_PTHREADS), 1)
OS_LIBS 	+= -lpthread -lrt
endif
ifeq ($(USE_IPV6), 1)
OS_LIBS		+= -lip6
endif
endif

ifeq ($(OS_ARCH),Linux)
ifeq ($(USE_PTHREADS), 1)
OS_LIBS		= -L/lib -lpthread -ldl -lc
else
OS_LIBS		= -L/lib -ldl -lc
endif
endif

ifeq ($(OS_ARCH),HP-UX)
ifeq ($(basename $(OS_RELEASE)),A.09)
OS_LIBS		= -ldld -L/lib/pa1.1 -lm
else
OS_LIBS		= -ldld -lm -lc
endif
ifeq ($(USE_PTHREADS), 1)
OS_LIBS 	+= -lpthread
endif
ifeq ($(PTHREADS_USER), 1)
OS_LIBS 	+= -lpthread
endif
endif

ifeq ($(OS_ARCH),UNIXWARE)
OS_LIBS		= -lsocket -lc
endif

ifeq ($(OS_ARCH),NEWS-OS)
OS_LIBS		= -lsocket -lnsl -lgen -lresolv
endif

ifeq ($(OS_ARCH),WINNT)
ifeq ($(OS_TARGET),OS2)
OS_LIBS		= so32dll.lib tcp32dll.lib
else
ifneq ($(OS_TARGET),WIN16)
OS_LIBS		= wsock32.lib winmm.lib
endif
endif
endif

#
# Define platform-dependent OBJS
#

OBJS = \
    io/$(OBJDIR)/prmwait.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prmapopt.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/priometh.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prlayer.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prlog.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prmmap.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prprf.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prscanf.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prstdio.$(OBJ_SUFFIX) \
    threads/$(OBJDIR)/prcmon.$(OBJ_SUFFIX) \
    linking/$(OBJDIR)/prlink.$(OBJ_SUFFIX) \
    malloc/$(OBJDIR)/prmalloc.$(OBJ_SUFFIX) \
    malloc/$(OBJDIR)/prmem.$(OBJ_SUFFIX) \
    md/$(OBJDIR)/prosdep.$(OBJ_SUFFIX) \
    memory/$(OBJDIR)/prseg.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/pralarm.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/pratom.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prdtoa.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prenv.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prerror.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prinit.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prinrval.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prlog2.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prlong.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prnetdb.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prsystem.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prthinfo.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prtime.$(OBJ_SUFFIX)

ifdef USE_PTHREADS
OBJS += \
    pthreads/$(OBJDIR)/ptsynch.$(OBJ_SUFFIX) \
    pthreads/$(OBJDIR)/ptio.$(OBJ_SUFFIX) \
    pthreads/$(OBJDIR)/ptthread.$(OBJ_SUFFIX) \
    pthreads/$(OBJDIR)/ptmisc.$(OBJ_SUFFIX)
else
OBJS += \
    io/$(OBJDIR)/prdir.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prfile.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prio.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prsocket.$(OBJ_SUFFIX) \
	threads/$(OBJDIR)/prcthr.$(OBJ_SUFFIX) \
	threads/$(OBJDIR)/prdump.$(OBJ_SUFFIX) \
	threads/$(OBJDIR)/prmon.$(OBJ_SUFFIX) \
	threads/$(OBJDIR)/prsem.$(OBJ_SUFFIX) \
	threads/$(OBJDIR)/prtpd.$(OBJ_SUFFIX) \
	threads/combined/$(OBJDIR)/prucpu.$(OBJ_SUFFIX) \
	threads/combined/$(OBJDIR)/prucv.$(OBJ_SUFFIX) \
	threads/combined/$(OBJDIR)/prulock.$(OBJ_SUFFIX) \
	threads/combined/$(OBJDIR)/prustack.$(OBJ_SUFFIX) \
	threads/combined/$(OBJDIR)/pruthr.$(OBJ_SUFFIX)
endif

ifeq ($(USE_IPV6), 1)
OBJS += io/$(OBJDIR)/pripv6.$(OBJ_SUFFIX)
endif

ifeq ($(OS_ARCH), WINNT)
ifneq (,$(filter-out WIN16 OS2,$(OS_TARGET)))
DLLBASE=/BASE:0x30000000
RES=$(OBJDIR)/nspr.res
RESNAME=nspr.rc
endif

ifeq ($(OS_TARGET), WIN16)
OBJS +=	md/windows/$(OBJDIR)/w16null.$(OBJ_SUFFIX) \
	md/windows/$(OBJDIR)/w16proc.$(OBJ_SUFFIX)      \
	md/windows/$(OBJDIR)/w16thred.$(OBJ_SUFFIX)      \
	md/windows/$(OBJDIR)/w16fmem.$(OBJ_SUFFIX)      \
	md/windows/$(OBJDIR)/w16sock.$(OBJ_SUFFIX)      \
	md/windows/$(OBJDIR)/w16mem.$(OBJ_SUFFIX)      \
    md/windows/$(OBJDIR)/w16io.$(OBJ_SUFFIX)       \
    md/windows/$(OBJDIR)/w16gc.$(OBJ_SUFFIX)       \
    md/windows/$(OBJDIR)/w16error.$(OBJ_SUFFIX)       \
    md/windows/$(OBJDIR)/w16callb.$(OBJ_SUFFIX)       \
	md/windows/$(OBJDIR)/ntinrval.$(OBJ_SUFFIX)
EXTRA_LIBS += $(MOD_DEPTH)/tools/winsock.lib
W16_EXPORTS = EXPORT _malloc.2=_PR_MD_malloc RESIDENT, \
	_realloc.3=_PR_MD_realloc RESIDENT, \
	_calloc.4=_PR_MD_calloc RESIDENT, \
	_free.5=_PR_MD_free RESIDENT, \
	_getenv.9=_PR_MD_getenv RESIDENT, \
	_printf.11=_PR_MD_printf RESIDENT, \
	_strftime.13=_PR_MD_strftime RESIDENT, \
	_sscanf.33=_PR_MD_sscanf RESIDENT, \
    _putenv.10=_PR_MD_putenv RESIDENT, \
    _fprintf.12=_PR_MD_fprintf RESIDENT
else
ifeq ($(OS_TARGET), WIN95)
OBJS +=	md/windows/$(OBJDIR)/w95io.$(OBJ_SUFFIX) \
	md/windows/$(OBJDIR)/w95sock.$(OBJ_SUFFIX) \
	md/windows/$(OBJDIR)/w95thred.$(OBJ_SUFFIX) \
	md/windows/$(OBJDIR)/w95cv.$(OBJ_SUFFIX) \
	md/windows/$(OBJDIR)/ntgc.$(OBJ_SUFFIX) \
	md/windows/$(OBJDIR)/ntmisc.$(OBJ_SUFFIX) \
	md/windows/$(OBJDIR)/ntinrval.$(OBJ_SUFFIX) \
	md/windows/$(OBJDIR)/ntsem.$(OBJ_SUFFIX)	\
	md/windows/$(OBJDIR)/win32_errors.$(OBJ_SUFFIX) \
	md/windows/$(OBJDIR)/w32poll.$(OBJ_SUFFIX)
else
ifeq ($(OS_TARGET), OS2)
OBJS +=	md/os2/$(OBJDIR)/os2io.$(OBJ_SUFFIX) \
	md/os2/$(OBJDIR)/os2sock.$(OBJ_SUFFIX) \
	md/os2/$(OBJDIR)/os2thred.$(OBJ_SUFFIX) \
	md/os2/$(OBJDIR)/os2cv.$(OBJ_SUFFIX) \
	md/os2/$(OBJDIR)/os2gc.$(OBJ_SUFFIX) \
	md/os2/$(OBJDIR)/os2misc.$(OBJ_SUFFIX) \
	md/os2/$(OBJDIR)/os2inrval.$(OBJ_SUFFIX) \
	md/os2/$(OBJDIR)/os2sem.$(OBJ_SUFFIX)	\
	md/os2/$(OBJDIR)/os2_errors.$(OBJ_SUFFIX) \
	md/os2/$(OBJDIR)/os2poll.$(OBJ_SUFFIX)
else
OBJS +=	md/windows/$(OBJDIR)/ntio.$(OBJ_SUFFIX) \
	md/windows/$(OBJDIR)/ntgc.$(OBJ_SUFFIX) \
	md/windows/$(OBJDIR)/ntthread.$(OBJ_SUFFIX) \
	md/windows/$(OBJDIR)/ntmisc.$(OBJ_SUFFIX) \
	md/windows/$(OBJDIR)/ntinrval.$(OBJ_SUFFIX) \
	md/windows/$(OBJDIR)/ntsem.$(OBJ_SUFFIX)	\
	md/windows/$(OBJDIR)/win32_errors.$(OBJ_SUFFIX) \
	md/windows/$(OBJDIR)/w32poll.$(OBJ_SUFFIX)
endif
endif
endif

else

	THREAD_DIR=threads/combined/$(OBJDIR)

ifeq ($(OS_ARCH), MAC)
	MD_DIR = md/mac/$(OBJDIR)
else
	MD_DIR = md/unix/$(OBJDIR)
	include md/unix/objs.mk
endif

endif

LIBRARY_NAME = nspr
LIBRARY_VERSION = $(MOD_VERSION)

RELEASE_LIBS = $(TARGETS)

include $(MOD_DEPTH)/config/rules.mk

ifeq ($(BUILD_AIX_RTL_LIBC),1)
TARGETS		+= $(AIX_RTL_LIBC)
# XXX is this a shared library?
endif

#
# The Client build wants the shared libraries in $(DIST)/bin,
# so we also install them there.
#

export:: $(TARGETS)
	$(INSTALL) -m 444 $(TARGETS) $(DIST)/lib
	$(INSTALL) -m 444 $(SHARED_LIBRARY) $(DIST)/bin
ifeq ($(MOZ_BITS),16)
	$(INSTALL) -m 444 $(TARGETS) $(MOZ_DIST)/lib
	$(INSTALL) -m 444 $(TARGETS) $(MOZ_DIST)/bin
endif

ifeq ($(BUILD_AIX_RTL_LIBC),1)
$(AIX_RTL_LIBC): /usr/ccs/lib/libc.a
	rtl_enable -o $@ $<
endif

install:: export
