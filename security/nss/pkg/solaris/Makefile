#
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
#ident	"$Id$"
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
