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
ifeq ($(OS_RELEASE),5.4_i86pc)
OS_LIBS                 = -lthread -lposix4 -lsocket -lnsl -lgen -lresolv -ldl
else
ifdef USE_PTHREADS
OS_LIBS			= -lpthread -lposix4 -lsocket -lnsl -ldl
else
ifdef LOCAL_THREADS_ONLY
OS_LIBS			= -lsocket -lnsl -ldl
else
OS_LIBS			= -lthread -lposix4 -lsocket -lnsl -ldl
endif	# LOCAL_THREADS_ONLY
endif	# USE_PTHREADS
endif	# 5.4_i86pc
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
ifeq ($(OS_RELEASE),A.09.03)
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
    io/$(OBJDIR)/prmwait.o \
    io/$(OBJDIR)/prmapopt.o \
    io/$(OBJDIR)/priometh.o \
    io/$(OBJDIR)/prlayer.o \
    io/$(OBJDIR)/prlog.o \
    io/$(OBJDIR)/prmmap.o \
    io/$(OBJDIR)/prprf.o \
    io/$(OBJDIR)/prscanf.o \
    io/$(OBJDIR)/prstdio.o \
    threads/$(OBJDIR)/prcmon.o \
    linking/$(OBJDIR)/prlink.o \
    malloc/$(OBJDIR)/prmalloc.o \
    malloc/$(OBJDIR)/prmem.o \
    md/$(OBJDIR)/prosdep.o \
    memory/$(OBJDIR)/prseg.o \
    misc/$(OBJDIR)/pralarm.o \
    misc/$(OBJDIR)/pratom.o \
    misc/$(OBJDIR)/prdtoa.o \
    misc/$(OBJDIR)/prenv.o \
    misc/$(OBJDIR)/prerror.o \
    misc/$(OBJDIR)/prinit.o \
    misc/$(OBJDIR)/prinrval.o \
    misc/$(OBJDIR)/prlog2.o \
    misc/$(OBJDIR)/prlong.o \
    misc/$(OBJDIR)/prnetdb.o \
    misc/$(OBJDIR)/prsystem.o \
    misc/$(OBJDIR)/prthinfo.o \
    misc/$(OBJDIR)/prtime.o

ifdef USE_PTHREADS
OBJS += \
    pthreads/$(OBJDIR)/ptsynch.o \
    pthreads/$(OBJDIR)/ptio.o \
    pthreads/$(OBJDIR)/ptthread.o \
    pthreads/$(OBJDIR)/ptmisc.o
else
OBJS += \
    io/$(OBJDIR)/prdir.o \
    io/$(OBJDIR)/prfile.o \
    io/$(OBJDIR)/prio.o \
    io/$(OBJDIR)/prsocket.o \
	threads/$(OBJDIR)/prcthr.o \
	threads/$(OBJDIR)/prdump.o \
	threads/$(OBJDIR)/prmon.o \
	threads/$(OBJDIR)/prsem.o \
	threads/$(OBJDIR)/prtpd.o \
	threads/combined/$(OBJDIR)/prucpu.o \
	threads/combined/$(OBJDIR)/prucv.o \
	threads/combined/$(OBJDIR)/prulock.o \
	threads/combined/$(OBJDIR)/prustack.o \
	threads/combined/$(OBJDIR)/pruthr.o
endif

ifeq ($(USE_IPV6), 1)
OBJS += io/$(OBJDIR)/pripv6.o
endif

ifeq ($(OS_ARCH), WINNT)
ifneq (,$(filter-out WIN16 OS2,$(OS_TARGET)))
DLLBASE=/BASE:0x30000000
RES=$(OBJDIR)/nspr.res
RESNAME=nspr.rc
endif

ifeq ($(OS_TARGET), WIN16)
OBJS +=	md/windows/$(OBJDIR)/w16null.o \
	md/windows/$(OBJDIR)/w16proc.o      \
	md/windows/$(OBJDIR)/w16thred.o      \
	md/windows/$(OBJDIR)/w16fmem.o      \
	md/windows/$(OBJDIR)/w16sock.o      \
	md/windows/$(OBJDIR)/w16mem.o      \
    md/windows/$(OBJDIR)/w16io.o       \
    md/windows/$(OBJDIR)/w16gc.o       \
    md/windows/$(OBJDIR)/w16error.o       \
    md/windows/$(OBJDIR)/w16callb.o       \
	md/windows/$(OBJDIR)/ntinrval.o
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
OBJS +=	md/windows/$(OBJDIR)/w95io.o \
	md/windows/$(OBJDIR)/w95sock.o \
	md/windows/$(OBJDIR)/w95thred.o \
	md/windows/$(OBJDIR)/w95cv.o \
	md/windows/$(OBJDIR)/ntgc.o \
	md/windows/$(OBJDIR)/ntmisc.o \
	md/windows/$(OBJDIR)/ntinrval.o \
	md/windows/$(OBJDIR)/ntsem.o	\
	md/windows/$(OBJDIR)/win32_errors.o \
	md/windows/$(OBJDIR)/w32poll.o
else
ifeq ($(OS_TARGET), OS2)
OBJS +=	md/os2/$(OBJDIR)/os2io.o \
	md/os2/$(OBJDIR)/os2sock.o \
	md/os2/$(OBJDIR)/os2thred.o \
	md/os2/$(OBJDIR)/os2cv.o \
	md/os2/$(OBJDIR)/os2gc.o \
	md/os2/$(OBJDIR)/os2misc.o \
	md/os2/$(OBJDIR)/os2inrval.o \
	md/os2/$(OBJDIR)/os2sem.o	\
	md/os2/$(OBJDIR)/os2_errors.o \
	md/os2/$(OBJDIR)/os2poll.o \
	md/os2/$(OBJDIR)/ramsem.o \
	md/os2/$(OBJDIR)/sem_x86.o
else
OBJS +=	md/windows/$(OBJDIR)/ntio.o \
	md/windows/$(OBJDIR)/ntgc.o \
	md/windows/$(OBJDIR)/ntthread.o \
	md/windows/$(OBJDIR)/ntmisc.o \
	md/windows/$(OBJDIR)/ntinrval.o \
	md/windows/$(OBJDIR)/ntsem.o	\
	md/windows/$(OBJDIR)/win32_errors.o \
	md/windows/$(OBJDIR)/w32poll.o
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
