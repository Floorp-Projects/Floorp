#
# Copyright 2002 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"$Id: Makefile,v 1.4 2004/06/10 18:28:19 christophe.ravel.bugs%sun.com Exp $"
#

CORE_DEPTH = ../../..

%: %.ksh
	$(RM) $@
	cp $< $@
	chmod +x $@


ifeq ($(USE_64), 1)
DIRS = \
	SUNWjssx
else
DIRS = \
	SUNWjss
endif

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

ifdef USE_64
$(ROOT)/usr/lib/mps/sparcv9: $(ROOT)/usr/lib
	echo "target=$(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_XPCLASS_JAR)"
	$(LN) -sf ../../../../$(DIST)/lib $@ 
	cp $(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_XPCLASS_JAR) $(ROOT)/usr/share/lib/mps/sparcv9/jss3.jar
else
$(ROOT)/usr/lib/mps: $(ROOT)/usr/lib
	echo "target=$(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_XPCLASS_JAR)"
	$(LN) -sf ../../../$(DIST)/lib $@
	cp $(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_XPCLASS_JAR) $(ROOT)/usr/share/lib/mps/jss3.jar
endif
