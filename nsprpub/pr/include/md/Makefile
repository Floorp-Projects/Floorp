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

MOD_DEPTH = ../../..

HEADERS = $(wildcard *.h)

include $(MOD_DEPTH)/config/rules.mk

ifeq ($(OS_ARCH),IRIX)
ifeq ($(USE_64), 1)
MDCPUCFG_H = _irix64.cfg
else
MDCPUCFG_H = _irix32.cfg
endif
endif

ifeq ($(OS_ARCH),WINNT)
ifeq ($(OS_TARGET), WIN95)
MDCPUCFG_H = _win95.cfg
else
ifeq ($(OS_TARGET), WIN16)
MDCPUCFG_H = _win16.cfg
else
ifeq ($(OS_TARGET),OS2)
MDCPUCFG_H = _os2.cfg
else
MDCPUCFG_H = _winnt.cfg
endif
endif
endif
endif

ifeq ($(OS_ARCH),AIX)
ifeq ($(USE_64),1)
MDCPUCFG_H = _aix64.cfg
else
MDCPUCFG_H = _aix32.cfg
endif
endif

ifeq ($(OS_ARCH),BSD_OS)
MDCPUCFG_H = _bsdi.cfg
endif

ifeq ($(OS_ARCH),FreeBSD)
MDCPUCFG_H = _freebsd.cfg
endif

ifeq ($(OS_ARCH),OpenBSD)
MDCPUCFG_H = _openbsd.cfg
endif

ifeq ($(OS_ARCH),NetBSD)
MDCPUCFG_H = _netbsd.cfg
endif

ifeq ($(OS_ARCH),HP-UX)
ifeq ($(USE_64), 1)
MDCPUCFG_H = _hpux64.cfg
else
MDCPUCFG_H = _hpux32.cfg
endif
endif

ifeq ($(OS_ARCH),Linux)
MDCPUCFG_H = _linux.cfg
endif

ifeq ($(OS_ARCH),OSF1)
MDCPUCFG_H = _osf1.cfg
endif

ifeq ($(OS_ARCH),SunOS)
ifeq ($(OS_RELEASE),4.1.3_U1)
MDCPUCFG_H = _sunos4.cfg
else
MDCPUCFG_H = _solaris.cfg
endif
endif

ifeq ($(OS_ARCH),SINIX)
MDCPUCFG_H = _reliantunix.cfg
endif

ifeq ($(OS_ARCH),Rhapsody)
MDCPUCFG_H = _rhapsody.cfg
endif

ifeq ($(OS_ARCH),NEXTSTEP)
MDCPUCFG_H = _nextstep.cfg
endif

ifeq ($(OS_ARCH),NEWS-OS)
MDCPUCFG_H = _sony.cfg
endif

ifeq ($(OS_ARCH),NEC)
MDCPUCFG_H = _nec.cfg
endif

ifeq ($(OS_ARCH),SCOOS)
MDCPUCFG_H = _scoos.cfg
endif

ifeq ($(OS_ARCH),UNIXWARE)
MDCPUCFG_H = _unixware.cfg
endif

ifeq ($(OS_ARCH),NCR)
MDCPUCFG_H = _ncr.cfg
endif

ifeq ($(OS_ARCH),DGUX)
MDCPUCFG_H = _dgux.cfg
endif

ifeq ($(OS_ARCH),QNX)
MDCPUCFG_H = _qnx.cfg
endif

ifeq ($(OS_ARCH),BeOS)
MDCPUCFG_H = _beos.cfg
endif

export:: $(HEADERS) $(MDCPUCFG_H)
	$(INSTALL) -m 444 $(HEADERS) $(DIST)/include/md
	$(INSTALL) -m 444 $(MDCPUCFG_H) $(DIST)/include
ifeq ($(MOZ_BITS),16)
	cp $(DIST)/include/$(MDCPUCFG_H) $(DIST)/include/prcpucfg.h
	$(INSTALL) -m 444 $(HEADERS) $(MOZ_INCL)/md
	$(INSTALL) -m 444 $(MDCPUCFG_H) $(MOZ_INCL)
	mv -f $(DIST)/include/$(MDCPUCFG_H) $(MOZ_INCL)/prcpucfg.h
else
	mv -f $(DIST)/include/$(MDCPUCFG_H) $(DIST)/include/prcpucfg.h
endif

release:: export
	@echo "Copying machine-dependent prcpucfg.h"
	@if test -z "$(BUILD_NUMBER)"; then \
		echo "BUILD_NUMBER must be defined"; \
		false; \
	fi
	@if test ! -d $(RELEASE_INCLUDE_DIR); then \
		rm -rf $(RELEASE_INCLUDE_DIR); \
		$(NSINSTALL) -D $(RELEASE_INCLUDE_DIR);\
	fi
	cp $(MDCPUCFG_H) $(RELEASE_INCLUDE_DIR)/prcpucfg.h
