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

MOD_DEPTH = ../../../..

include $(MOD_DEPTH)/config/config.mk

# Disable optimization of the nspr on SunOS4.1.3
ifeq ($(OS_ARCH),SunOS)
ifeq ($(OS_RELEASE),4.1.3_U1)
OPTIMIZER =
endif
endif

CSRCS =          \
	unix.c    \
	unix_errors.c    \
	uxproces.c \
	uxwrap.c \
	$(NULL)

PTH_USER_CSRCS =          \
	pthreads_user.c \
	$(NULL)

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

BSDI_CSRCS = \
	bsdi.c \
	$(NULL)

HPUX_CSRCS = \
	hpux.c \
	$(NULL)

OSF1_CSRCS = \
	osf1.c \
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

ifeq ($(PTHREADS_USER),1)
CSRCS += $(PTH_USER_CSRCS)
endif

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
ifeq ($(OS_ARCH),BSD_386)
CSRCS += $(BSDI_CSRCS)
endif
ifeq ($(OS_ARCH),HP-UX)
CSRCS += $(HPUX_CSRCS)
endif
ifeq ($(OS_ARCH),OSF1)
CSRCS += $(OSF1_CSRCS)
endif
ifeq ($(OS_ARCH),Linux)
CSRCS += $(LINUX_CSRCS)
endif
ifeq ($(OS_ARCH),UNIXWARE)
CSRCS += $(UNIXWARE_CSRCS)
endif
ifeq ($(OS_ARCH),ReliantUNIX)
CSRCS += $(RELIANTUNIX_CSRCS)
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
ifeq ($(OS_ARCH),SCO_SV)
CSRCS += $(SCOOS_CSRCS)
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
    ifeq ($(CPU_ARCH),x86)
	ASFILES = os_$(OS_ARCH)_x86.s
    else
	ifneq ($(OS_RELEASE),4.1.3_U1)
		ASFILES = os_$(OS_ARCH).s
	endif
    endif
endif

ifeq ($(OS_ARCH), ReliantUNIX)
    ASFILES   = os_$(OS_ARCH).s
endif

ifeq ($(OS_ARCH)$(OS_RELEASE),BSD_3862.1)
    ASFILES = os_BSD_386_2.s
endif

TARGETS		= $(OBJS)

ifeq ($(OS_ARCH),AIX)
ifneq ($(OS_RELEASE),4.2)
ifneq ($(USE_PTHREADS), 1)
#TARGETS		+= $(OBJDIR)/aixwrap.$(OBJ_SUFFIX)
endif
endif
endif

ifeq ($(OS_ARCH),SunOS)
	ifneq ($(OS_RELEASE),4.1.3_U1)
    	ifeq ($(OS_TEST),sun4u)
			LIBRARY_NAME = $(ULTRASPARC_LIBRARY)
			LIBRARY_VERSION = $(MOD_VERSION)
            ULTRASPARC_ASFILES = os_$(OS_ARCH)_ultrasparc.s
            ULTRASPARC_ASOBJS = $(addprefix $(OBJDIR)/,$(ULTRASPARC_ASFILES:.s=.$(OBJ_SUFFIX)))
			TARGETS		+= $(ULTRASPARC_ASOBJS) $(SHARED_LIBRARY)
		endif
	endif
endif

INCLUDES = -I$(DIST)/include/private -I$(DIST)/include

include $(MOD_DEPTH)/config/rules.mk

export:: $(TARGETS)
#ifeq ($(OS_ARCH),AIX)
#ifneq ($(OS_RELEASE),4.2)
#ifneq ($(USE_PTHREADS), 1)
#	$(INSTALL) -m 444 $(OBJDIR)/aixwrap.$(OBJ_SUFFIX) $(DIST)/lib
#endif
#endif
#endif
ifeq ($(OS_ARCH),SunOS)
ifneq ($(OS_RELEASE),4.1.3_U1)
ifeq ($(OS_TEST),sun4u)
$(SHARED_LIBRARY): $(ULTRASPARC_ASOBJS)
	$(LD) -G -z text -o $@ $(ULTRASPARC_ASOBJS)
	$(INSTALL) -m 444 $(SHARED_LIBRARY) $(DIST)/lib

$(ULTRASPARC_ASOBJS): $(ULTRASPARC_ASFILES)
	as -o $@ -K PIC -P -D_ASM -D__STDC__=0 -xarch=v8plus $(ULTRASPARC_ASFILES)
endif
endif
endif


install:: export
