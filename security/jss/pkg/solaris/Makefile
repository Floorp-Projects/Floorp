#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"$Id: Makefile,v 1.6 2004/06/15 03:50:17 christophe.ravel.bugs%sun.com Exp $"
#

CORE_DEPTH = ../../..

%: %.ksh
	$(RM) $@
	$(CP) $< $@
	chmod +x $@


DIRS = \
	SUNWjss

PROTO = \
	$(ROOT) \
	$(ROOT)/usr/share/lib/mps \
	$(ROOT)/usr/lib/mps

ifeq ($(ARCH),sparc)
	PROTO += $(ROOT)/usr/share/lib/mps/sparcv9 \
	$(ROOT)/usr/lib/mps/sparcv9
endif

include Makefile.com

DIST64 = $(shell echo $(DIST) | sed -e "s|_OPT|_64_OPT|g" -e "s|_DBG|_64_DBG|g")

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

$(ROOT)/usr/lib/mps/sparcv9:
	mkdir -p $@
	$(CP) -r $(DIST64)/lib/*.so $@ 
$(ROOT)/usr/share/lib/mps/sparcv9:
	echo "target=$(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_XPCLASS_JAR)"
	mkdir -p $@
	$(CP) $(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_XPCLASS_JAR) $@/jss3.jar
$(ROOT)/usr/lib/mps:
	mkdir -p $@
	$(CP) -r $(DIST)/lib/*.so $@
$(ROOT)/usr/share/lib/mps:
	echo "target=$(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_XPCLASS_JAR)"
	mkdir -p $@
	$(CP) $(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_XPCLASS_JAR) $@/jss3.jar

