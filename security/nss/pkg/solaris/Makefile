#
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Netscape security libraries.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1994-2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
#
#ident	"$Id: Makefile,v 1.6 2005/02/26 02:20:40 christophe.ravel.bugs%sun.com Exp $"
#

CORE_DEPTH = ../../..

%: %.ksh
	$(RM) $@
	cp $< $@
	chmod +x $@

HEADER_DIR = public/nss

DIRS = \
	SUNWtls \
	SUNWtlsu \
	SUNWtlsd

include Makefile.com

PROTO = \
	$(ROOT) \
	$(ROOT)/usr/lib/mps \
	$(ROOT)/usr/include/mps \
	$(ROOT)/usr/sfw/bin

ifeq ($(MACH), sparc)
	PROTO += $(ROOT)/usr/lib/mps/sparcv9 \
	$(ROOT)/usr/sfw/bin/sparcv9
endif

ifeq ($(USE_64), 1)
ifeq ($(MACH), sparc)
# Sparc
	PROTO += $(ROOT)/usr/lib/mps/sparcv9 \
	$(ROOT)/usr/sfw/bin/sparcv9
else
# AMD64
	PROTO += $(ROOT)/usr/lib/mps/amd64 \
	$(ROOT)/usr/sfw/bin/amd64
endif
	DIST64 = $(DIST)
	DIST32 = $(shell echo $(DIST) | sed -e "s|_64_OPT|_OPT|g" -e "s|_64_DBG|_DBG|g")
else
	DIST32 = $(DIST)
	DIST64 = $(shell echo $(DIST) | sed -e "s|_OPT|_64_OPT|g" -e "s|_DBG|_64_DBG|g")
endif

awk_pkginfo: bld_awk_pkginfo
	./bld_awk_pkginfo -m $(MACH) -p "$(PRODUCT_VERSION)" -o $@ -v $(PRODUCT_VERSION)

all:: awk_pkginfo $(PROTO)
publish: awk_pkginfo $(PROTO)
	+$(LOOP_OVER_DIRS)

clean clobber::
	$(RM) awk_pkginfo bld_awk_pkginfo
	$(RM) -r $(ROOT)

$(ROOT):
	mkdir -p $@

$(ROOT)/usr/lib/mps:
	mkdir -p $@
	$(CP) -r $(DIST32)/lib/*.so $@
	$(CP) -r $(DIST32)/lib/*.chk $@
$(ROOT)/usr/sfw/bin:
	mkdir -p $@
	-$(CP) -r $(DIST32)/bin/* $@
$(ROOT)/usr/include/mps:
	mkdir -p $@
	$(CP) -r $(SOURCE_PREFIX)/$(HEADER_DIR)/*.h $@
$(ROOT)/usr/lib/mps/sparcv9:
	mkdir -p $@
	$(CP) -r $(DIST64)/lib/*.so $@
	$(CP) -r $(DIST64)/lib/*.chk $@
$(ROOT)/usr/sfw/bin/sparcv9:
	mkdir -p $@
	$(CP) -r $(DIST64)/bin/* $@
$(ROOT)/usr/lib/mps/amd64:
	mkdir -p $@
	$(CP) -r $(DIST64)/lib/*.so $@
	$(CP) -r $(DIST64)/lib/*.chk $@
$(ROOT)/usr/sfw/bin/amd64:
	mkdir -p $@
	$(CP) -r $(DIST64)/bin/* $@
