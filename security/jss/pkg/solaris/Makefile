#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"$Id: Makefile,v 1.5 2004/06/14 23:24:57 christophe.ravel.bugs%sun.com Exp $"
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
	$(ROOT)/usr \
	$(ROOT)/usr/share \
	$(ROOT)/usr/share/lib \
	$(ROOT)/usr/share/lib/mps \
	$(ROOT)/usr/share/lib/mps/sparcv9 \
	$(ROOT)/usr/lib \
	$(ROOT)/usr/lib/mps \
	$(ROOT)/usr/lib/mps/sparcv9

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

$(ROOT) $(ROOT)/%:
	mkdir -p $@

$(ROOT)/usr/lib/mps/sparcv9: $(ROOT)/usr/lib
	echo "target=$(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_XPCLASS_JAR)"
	mkdir -p $@
	$(CP) -r $(DIST64)/lib/*.so $@ 
	$(CP) $(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_XPCLASS_JAR) $(ROOT)/usr/share/lib/mps/sparcv9/jss3.jar
$(ROOT)/usr/lib/mps: $(ROOT)/usr/lib
	echo "target=$(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_XPCLASS_JAR)"
	mkdir -p $@
	$(CP) -r $(DIST)/lib/*.so $@
	$(CP) $(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_XPCLASS_JAR) $(ROOT)/usr/share/lib/mps/jss3.jar
