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

# This makefile appends to the variable OBJS the platform-dependent
# object modules that will be part of the nspr20 library.

CSRCS =          \
	unix.c    \
	unix_errors.c \
	uxproces.c \
	uxrng.c \
	uxshm.c \
	uxwrap.c \
	$(NULL)

ifneq ($(USE_PTHREADS),1)
CSRCS += uxpoll.c
endif

ifeq ($(PTHREADS_USER),1)
CSRCS += pthreads_user.c
endif

ifdef USE_AUTOCONF

CSRCS	+= $(PR_MD_CSRCS)
ASFILES += $(PR_MD_ASFILES)

else # ! USE_AUTOCONF
IRIX_CSRCS =	 \
	irix.c	 \
	$(NULL)

SUNOS4_CSRCS =	 \
	sunos4.c	 \
	$(NULL)

SOLARIS_CSRCS = \
	solaris.c	\
	$(NULL)

AIX_CSRCS =	\
	aix.c	\
	$(NULL)

FREEBSD_CSRCS = \
	freebsd.c \
	$(NULL)

NETBSD_CSRCS = \
	netbsd.c \
	$(NULL)

OPENBSD_CSRCS = \
	openbsd.c \
	$(NULL)

BSDI_CSRCS = \
	bsdi.c \
	$(NULL)

HPUX_CSRCS = \
	hpux.c \
	$(NULL)

OSF1_CSRCS = \
	osf1.c \
	$(NULL)

OPENVMS_CSRCS = \
	openvms.c \
	$(NULL)

LINUX_CSRCS = \
	linux.c \
	$(NULL)

UNIXWARE_CSRCS = \
	unixware.c \
	$(NULL)

RELIANTUNIX_CSRCS = \
	reliantunix.c \
	$(NULL)

RHAPSODY_CSRCS = \
	rhapsody.c \
	$(NULL)

NEXTSTEP_CSRCS = \
	nextstep.c \
	$(NULL)

NEC_CSRCS = \
	nec.c \
	$(NULL)

SONY_CSRCS = \
	sony.c \
	$(NULL)

NCR_CSRCS = \
	ncr.c \
	$(NULL)

SCOOS_CSRCS = \
	scoos.c \
	$(NULL)

DGUX_CSRCS = \
	dgux.c \
	$(NULL)

QNX_CSRCS = \
	qnx.c \
	$(NULL)

NTO_CSRCS = \
	nto.c \
	$(NULL)

ifeq ($(OS_ARCH),IRIX)
CSRCS += $(IRIX_CSRCS)
endif

ifeq ($(OS_ARCH),SunOS)
ifeq ($(OS_RELEASE),4.1.3_U1)
CSRCS += $(SUNOS4_CSRCS)
else
CSRCS += $(SOLARIS_CSRCS)
endif
endif

ifeq ($(OS_ARCH),AIX)
CSRCS += $(AIX_CSRCS)
endif
ifeq ($(OS_ARCH),FreeBSD)
CSRCS += $(FREEBSD_CSRCS)
endif
ifeq ($(OS_ARCH),NetBSD)
CSRCS += $(NETBSD_CSRCS)
endif
ifeq ($(OS_ARCH),OpenBSD)
CSRCS += $(OPENBSD_CSRCS)
endif
ifeq ($(OS_ARCH),BSD_OS)
CSRCS += $(BSDI_CSRCS)
endif
ifeq ($(OS_ARCH),HP-UX)
CSRCS += $(HPUX_CSRCS)
endif
ifeq ($(OS_ARCH),OSF1)
CSRCS += $(OSF1_CSRCS)
endif
ifeq ($(OS_ARCH),OpenVMS)
CSRCS += $(OPENVMS_CSRCS)
endif
ifeq ($(OS_ARCH),Linux)
CSRCS += $(LINUX_CSRCS)
endif
ifeq ($(OS_ARCH),UNIXWARE)
CSRCS += $(UNIXWARE_CSRCS)
endif
ifeq ($(OS_ARCH),SINIX)
CSRCS += $(RELIANTUNIX_CSRCS)
endif
ifeq ($(OS_ARCH),Rhapsody)
CSRCS += $(RHAPSODY_CSRCS)
endif
ifeq ($(OS_ARCH),NEXTSTEP)
CSRCS += $(NEXTSTEP_CSRCS)
endif
ifeq ($(OS_ARCH),NEC)
CSRCS += $(NEC_CSRCS)
endif
ifeq ($(OS_ARCH),NEWS-OS)
CSRCS += $(SONY_CSRCS)
endif
ifeq ($(OS_ARCH),NCR)
CSRCS += $(NCR_CSRCS)
endif
ifeq ($(OS_ARCH),SCOOS)
CSRCS += $(SCOOS_CSRCS)
endif
ifeq ($(OS_ARCH),DGUX)
CSRCS += $(DGUX_CSRCS)
endif
ifeq ($(OS_ARCH),NTO)
CSRCS += $(NTO_CSRCS)
endif
ifeq ($(OS_ARCH),QNX)
ifeq ($(OS_TARGET),NTO)
CSRCS += $(NTO_CSRCS)
else
CSRCS += $(QNX_CSRCS)
endif
endif
 
#
# Some Unix platforms have an assembly language file.
# E.g., AIX 3.2, Solaris (both sparc and x86).
#
ifeq ($(OS_ARCH), AIX)
    ifeq ($(OS_RELEASE), 3.2)
	ASFILES   = os_$(OS_ARCH).s
    endif
endif

ifeq ($(OS_ARCH),SunOS)
    ifneq ($(OS_RELEASE),4.1.3_U1)
    ifneq ($(LOCAL_THREADS_ONLY),1)
        ifeq ($(CPU_ARCH),x86)
            ASFILES = os_$(OS_ARCH)_x86.s
        else
            ASFILES = os_$(OS_ARCH).s
            ifneq ($(USE_64),1)
            ASFILES += os_$(OS_ARCH)_32.s
            endif
        endif
    endif
    endif
endif

ifeq ($(OS_ARCH),Linux)
    ifeq ($(CPU_ARCH),x86)
        ASFILES   = os_Linux_x86.s
    endif
endif

ifeq ($(OS_ARCH), SINIX)
    ifeq ($(CPU_ARCH),mips)
        ASFILES   = os_ReliantUNIX.s
    endif
endif

ifeq ($(OS_ARCH), HP-UX)
    ASFILES   = os_HPUX.s
endif

ifeq ($(OS_ARCH), IRIX)
    ASFILES   = os_Irix.s
endif

ifeq ($(OS_ARCH)$(OS_RELEASE),BSD_OS2.1)
    ASFILES = os_BSD_386_2.s
endif

endif # !USE_AUTOCONF

OBJS += $(addprefix md/unix/$(OBJDIR)/,$(CSRCS:.c=.$(OBJ_SUFFIX)))  \
	$(addprefix md/unix/$(OBJDIR)/,$(ASFILES:.s=.$(OBJ_SUFFIX)))

