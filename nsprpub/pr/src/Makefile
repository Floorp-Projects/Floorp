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

MOD_DEPTH = ../..

include $(MOD_DEPTH)/config/config.mk

DIRS = io linking malloc md memory misc threads

ifeq ($(USE_PTHREADS), 1)
	DIRS += pthreads
endif

ifeq ($(USE_BTHREADS), 1)
	DIRS += bthreads
endif

ifeq ($(USE_CPLUS), 1)
	DIRS += cplus
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
OS_LIBS			= -lpthread -lthread -lposix4 -lsocket -lnsl -ldl
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
OS_LIBS = -lpthread
endif
endif

ifeq ($(OS_ARCH),AIX)
ifeq ($(CLASSIC_NSPR),1)
ifeq ($(OS_RELEASE),4.1)
OS_LIBS		= -lsvld -lc
else
OS_LIBS		= -ldl -lc
endif
else
ifeq ($(OS_RELEASE),4.1)
OS_LIBS		= -lpthreads -lsvld -lC_r -lC -lc_r -lm /usr/lib/libc.a
else
OS_LIBS		= -lpthreads -ldl -lC_r -lC -lc_r -lm /usr/lib/libc.a
endif
endif
endif

# On AIX, we override malloc in non-pthread versions.  On AIX 4.2 or
# above, this requires that we use the rtl-enabled version of libc.a.
ifeq ($(OS_ARCH),AIX)
ifneq (,$(filter-out 3.2 4.1,$(OS_RELEASE)))
ifneq ($(USE_PTHREADS),1)
BUILD_AIX_RTL_LIBC = 1
AIX_RTL_LIBC	= $(OBJDIR)/libc.a
endif
endif
endif

ifeq ($(OS_ARCH),OSF1)
ifeq ($(USE_PTHREADS), 1)
OS_LIBS 	= -lpthread -lrt
endif
ifneq ($(OS_RELEASE),V2.0)
OS_LIBS		+= -lc_r
endif
ifeq ($(USE_IPV6), 1)
OS_LIBS		+= -lip6
endif
endif

ifeq ($(OS_ARCH),Linux)
ifeq ($(USE_PTHREADS), 1)
OS_LIBS		= -lpthread -ldl -lc
else
OS_LIBS		= -ldl -lc
endif
endif

ifeq ($(OS_ARCH),HP-UX)
ifeq ($(USE_PTHREADS), 1)
ifeq (,$(filter-out B.10.10 B.10.20,$(OS_RELEASE)))
OS_LIBS 	= -ldce
else
OS_LIBS 	= -lpthread
endif
endif
ifeq ($(PTHREADS_USER), 1)
OS_LIBS 	= -lpthread
endif
ifeq ($(basename $(OS_RELEASE)),A.09)
OS_LIBS		+= -ldld -L/lib/pa1.1 -lm
else
OS_LIBS		+= -ldld -lm -lc
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
# We define this in os2.mk.
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
    $(OBJDIR)/prvrsion.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prfdcach.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prmwait.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prmapopt.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/priometh.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prlayer.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prlog.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prmmap.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prpolevt.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prprf.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prscanf.$(OBJ_SUFFIX) \
    io/$(OBJDIR)/prstdio.$(OBJ_SUFFIX) \
    threads/$(OBJDIR)/prcmon.$(OBJ_SUFFIX) \
	threads/$(OBJDIR)/prrwlock.$(OBJ_SUFFIX) \
	threads/$(OBJDIR)/prtpd.$(OBJ_SUFFIX) \
    linking/$(OBJDIR)/prlink.$(OBJ_SUFFIX) \
    malloc/$(OBJDIR)/prmalloc.$(OBJ_SUFFIX) \
    malloc/$(OBJDIR)/prmem.$(OBJ_SUFFIX) \
    md/$(OBJDIR)/prosdep.$(OBJ_SUFFIX) \
    memory/$(OBJDIR)/prseg.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/pralarm.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/pratom.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prcountr.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prdtoa.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prenv.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prerr.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prerror.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prerrortable.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prinit.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prinrval.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prlog2.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prlong.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prnetdb.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prolock.$(OBJ_SUFFIX)	 \
    misc/$(OBJDIR)/prsystem.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prthinfo.$(OBJ_SUFFIX) \
    misc/$(OBJDIR)/prtrace.$(OBJ_SUFFIX) \
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
    io/$(OBJDIR)/prsocket.$(OBJ_SUFFIX)

ifndef USE_BTHREADS
OBJS += \
	threads/$(OBJDIR)/prcthr.$(OBJ_SUFFIX) \
	threads/$(OBJDIR)/prdump.$(OBJ_SUFFIX) \
	threads/$(OBJDIR)/prmon.$(OBJ_SUFFIX) \
	threads/$(OBJDIR)/prsem.$(OBJ_SUFFIX) \
	threads/combined/$(OBJDIR)/prucpu.$(OBJ_SUFFIX) \
	threads/combined/$(OBJDIR)/prucv.$(OBJ_SUFFIX) \
	threads/combined/$(OBJDIR)/prulock.$(OBJ_SUFFIX) \
	threads/combined/$(OBJDIR)/prustack.$(OBJ_SUFFIX) \
	threads/combined/$(OBJDIR)/pruthr.$(OBJ_SUFFIX)
endif

endif

ifeq ($(USE_IPV6), 1)
OBJS += io/$(OBJDIR)/pripv6.$(OBJ_SUFFIX)
endif

ifeq ($(USE_CPLUS), 1)
OBJS += \
	cplus/$(OBJDIR)/rcbase.$(OBJ_SUFFIX) \
	cplus/$(OBJDIR)/rccv.$(OBJ_SUFFIX) \
	cplus/$(OBJDIR)/rcfileio.$(OBJ_SUFFIX) \
	cplus/$(OBJDIR)/rcinrval.$(OBJ_SUFFIX) \
	cplus/$(OBJDIR)/rcio.$(OBJ_SUFFIX) \
	cplus/$(OBJDIR)/rclock.$(OBJ_SUFFIX) \
	cplus/$(OBJDIR)/rcnetdb.$(OBJ_SUFFIX) \
	cplus/$(OBJDIR)/rcnetio.$(OBJ_SUFFIX) \
	cplus/$(OBJDIR)/rcthread.$(OBJ_SUFFIX) \
	cplus/$(OBJDIR)/rctime.$(OBJ_SUFFIX)
endif

ifeq ($(OS_ARCH), WINNT)
ifneq (,$(filter-out WIN16 OS2,$(OS_TARGET)))
DLLBASE=/BASE:0x30000000
RES=$(OBJDIR)/nspr.res
RESNAME=nspr.rc
ifdef MOZ_DEBUG
ifdef GLOWCODE
EXTRA_LIBS += $(GLOWDIR)/glowcode.lib
endif
endif
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
	md/windows/$(OBJDIR)/w32poll.$(OBJ_SUFFIX)	\
	md/windows/$(OBJDIR)/w95dllmain.$(OBJ_SUFFIX)
else
ifeq ($(OS_TARGET),OS2)
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
OBJS +=	md/windows/$(OBJDIR)/ntdllmn.$(OBJ_SUFFIX) \
	md/windows/$(OBJDIR)/ntio.$(OBJ_SUFFIX) \
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

ifeq ($(OS_ARCH), BeOS)
	include md/beos/objs.mk
	include bthreads/objs.mk
else
	# Unix
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
# Version information generation (begin)
#
ECHO = echo
INCLUDES = -I$(DIST)/include
TINC = $(OBJDIR)/_pr_bld.h
PROD = $(notdir $(SHARED_LIBRARY))
NOW = $(MOD_DEPTH)/config/$(OBJDIR)/now
SH_DATE = $(shell date)
SH_NOW = $(shell $(NOW))

ifeq ($(OS_ARCH), WINNT)
ifeq ($(OS_TARGET),OS2)
	SUF =
else
	SUF = i64
endif
else
	SUF = LL
endif

$(TINC):
	@$(MAKE_OBJDIR)
	@$(ECHO) '#define _BUILD_STRING "$(SH_DATE)"' > $(TINC)
	@if test ! -z "$(SH_NOW)"; then \
	    $(ECHO) '#define _BUILD_TIME $(SH_NOW)$(SUF)' >> $(TINC); \
	else \
	    true; \
	fi
	@$(ECHO) '#define _PRODUCTION "$(PROD)"' >> $(TINC)


$(OBJDIR)/prvrsion.$(OBJ_SUFFIX): prvrsion.c $(TINC)
ifeq ($(OS_ARCH), WINNT)
ifdef XP_OS2_EMX
	$(CC) -o $@ -c $(CFLAGS) -I$(OBJDIR) prvrsion.c
else
	$(CC) -Fo$@ -c $(CFLAGS) -I$(OBJDIR) prvrsion.c
endif
else
	$(CC) -o $@ -c $(CFLAGS) -I$(OBJDIR) prvrsion.c
endif
#
# Version information generation (end)
#


#
# The Client build wants the shared libraries in $(DIST)/bin,
# so we also install them there.
#

export:: $(TARGETS)
	$(INSTALL) -m 444 $(TARGETS) $(DIST)/lib
ifdef SHARED_LIBRARY
	$(INSTALL) -m 444 $(SHARED_LIBRARY) $(DIST)/bin
endif
ifeq ($(MOZ_BITS),16)
	$(INSTALL) -m 444 $(TARGETS) $(MOZ_DIST)/lib
	$(INSTALL) -m 444 $(TARGETS) $(MOZ_DIST)/bin
endif

ifeq ($(BUILD_AIX_RTL_LIBC),1)
$(AIX_RTL_LIBC): /usr/ccs/lib/libc.a
	rtl_enable -o $@ $<
endif

install:: export
