#
# Copyright 2002 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"$Id: Makefile,v 1.3 2002/11/11 20:44:54 kirk.erickson%sun.com Exp $"
#

CORE_DEPTH = ../../..

%: %.ksh
	$(RM) $@
	cp $< $@
	chmod +x $@


ifeq ($(USE_64), 1)
DIRS = \
    SUNWtlsx
else
DIRS = \
    SUNWtls
endif

PROTO = \
	$(ROOT) \
	$(ROOT)/usr \
	$(ROOT)/usr/lib \
	$(ROOT)/usr/lib/mps

ifdef USE_64
PROTO += $(ROOT)/usr/lib/mps/sparcv9
endif

include Makefile.com

awk_pkginfo-$(MACH): bld_awk_pkginfo
	./bld_awk_pkginfo -m $(MACH) -p "$(PRODUCT_VERSION)" -o $@ -v $(PRODUCT_VERSION)

all:: awk_pkginfo-$(MACH) $(PROTO)
publish: awk_pkginfo-$(MACH) $(PROTO)
	+$(LOOP_OVER_DIRS)

clean clobber::
	$(RM) awk_pkginfo-$(MACH) bld_awk_pkginfo
	$(RM) -r $(ROOT)

$(ROOT) $(ROOT)/%:
	mkdir -p $@

ifdef USE_64
$(ROOT)/usr/lib/mps/sparcv9: $(ROOT)/usr/lib
	$(LN) -sf ../../../../$(DIST)/lib $@
else
$(ROOT)/usr/lib/mps: $(ROOT)/usr/lib
	$(LN) -sf ../../../$(DIST)/lib $@
endif
