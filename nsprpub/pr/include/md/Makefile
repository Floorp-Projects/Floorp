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
MDCPUCFG_H = _winnt.cfg
endif
endif
endif

ifeq ($(OS_ARCH),OS2)
MDCPUCFG_H = _os2.cfg
endif

ifeq ($(OS_ARCH),AIX)
ifeq ($(USE_64),1)
MDCPUCFG_H = _aix64.cfg
else
ifeq ($(USE_IPV6),1)
MDCPUCFG_H = _aix32in6.cfg
else
MDCPUCFG_H = _aix32.cfg
endif
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

ifeq ($(OS_ARCH),OpenVMS)
MDCPUCFG_H = _openvms.cfg
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
ifeq ($(USE_64),1)
MDCPUCFG_H = _solaris64.cfg
else
MDCPUCFG_H = _solaris32.cfg
endif
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
ifeq (,$(filter-out 2.1,$(OS_RELEASE)))
MDCPUCFG_H = _unixware.cfg
else
MDCPUCFG_H = _unixware7.cfg
endif
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

ifeq ($(OS_TARGET),NTO)
MDCPUCFG_H = _nto.cfg
endif

ifeq ($(OS_ARCH),BeOS)
MDCPUCFG_H = _beos.cfg
endif

export:: $(MDCPUCFG_H)
	$(INSTALL) -m 444 $(MDCPUCFG_H) $(DIST)/include
ifeq ($(MOZ_BITS),16)
	cp $(DIST)/include/$(MDCPUCFG_H) $(DIST)/include/prcpucfg.h
	$(INSTALL) -m 444 $(MDCPUCFG_H) $(MOZ_INCL)
	mv -f $(DIST)/include/$(MDCPUCFG_H) $(MOZ_INCL)/prcpucfg.h
else
ifneq ($(OS_ARCH),OpenVMS)
	mv -f $(DIST)/include/$(MDCPUCFG_H) $(DIST)/include/prcpucfg.h
else
# mv'ing a link causes the file itself to move, not the link.
	rm -f $(DIST)/include/$(MDCPUCFG_H)
	rm -f $(DIST)/include/prcpucfg.h
	ln -fs `pwd`/$(MDCPUCFG_H) $(DIST)/include/prcpucfg.h
endif
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
