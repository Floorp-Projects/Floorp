#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"$Id: Makefile,v 1.8 2004/07/20 20:39:25 glen.beasley%sun.com Exp $"
#

CORE_DEPTH = ../../..

%: %.ksh
	$(RM) $@
	$(CP) $< $@
	chmod +x $@


DIRS = \
	SUNWjss

include Makefile.com

PROTO = \
	$(ROOT) \
	$(ROOT)/usr/share/lib/mps \
	$(ROOT)/usr/lib/mps

ifeq ($(MACH),sparc)
	PROTO += $(ROOT)/usr/share/lib/mps/sparcv9 \
	$(ROOT)/usr/lib/mps/sparcv9
endif

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
	$(CP) $(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_XPCLASS_JAR) $@/jss4.jar
$(ROOT)/usr/lib/mps:
	mkdir -p $@
	$(CP) -r $(DIST)/lib/*.so $@
$(ROOT)/usr/share/lib/mps:
	echo "target=$(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_XPCLASS_JAR)"
	mkdir -p $@
	$(CP) $(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_XPCLASS_JAR) $@/jss4.jar

