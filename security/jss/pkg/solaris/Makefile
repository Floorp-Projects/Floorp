#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"$Id: Makefile,v 1.12 2004/10/09 01:53:26 christophe.ravel.bugs%sun.com Exp $"
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

ifeq ($(USE_64), 1)
ifeq ($(MACH), sparc)
# Sparc
	PROTO += $(ROOT)/usr/lib/mps/sparcv9 \
	$(ROOT)/usr/share/lib/mps/sparcv9
else
# AMD64
	PROTO += $(ROOT)/usr/lib/mps/amd64 \
	$(ROOT)/usr/share/lib/mps/amd64
endif
	DIST64 = $(DIST)
	DIST32 = $(shell echo $(DIST) | sed -e "s|_64_OPT|_OPT|g" -e "s|_64_DBG|_DBG|g")
else
	DIST32 = $(DIST)
	DIST64 = $(shell echo $(DIST) | sed -e "s|_OPT|_64_OPT|g" -e "s|_DBG|_64_DBG|g")
endif

OBJ32 = $(shell basename $(DIST32))
OBJ64 = $(shell basename $(DIST64))

ifeq ($(BUILD_OPT),1)
  IMPORT_JSS_JAR = jss4.jar
else
  IMPORT_JSS_JAR = jss4_dbg.jar
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

$(ROOT)/usr/lib/mps/sparcv9:
	mkdir -p $@
	$(CP) -r $(DIST64)/lib/*.so $@ 
ifdef JSS3_LOCATION
	$(CP) $(JSS3_LOCATION)/$(OBJ64)/lib/libjss3.so $@
endif

$(ROOT)/usr/share/lib/mps/sparcv9:
	echo "target=$(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_JSS_JAR)"
	mkdir -p $@
	$(CP) $(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_JSS_JAR) $@/jss4.jar
ifdef JSS3_LOCATION
	$(CP) $(JSS3_LOCATION)/$(IMPORT_XPCLASS_JAR) $@/jss3.jar
endif

$(ROOT)/usr/lib/mps/amd64:
	mkdir -p $@
	$(CP) -r $(DIST64)/lib/*.so $@ 

$(ROOT)/usr/share/lib/mps/amd64:
	echo "target=$(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_JSS_JAR)"
	mkdir -p $@
	$(CP) $(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_JSS_JAR) $@/jss4.jar

$(ROOT)/usr/lib/mps:
	mkdir -p $@
	$(CP) -r $(DIST32)/lib/*.so $@
ifdef JSS3_LOCATION
	$(CP) $(JSS3_LOCATION)/$(OBJ32)/lib/libjss3.so $@
endif

$(ROOT)/usr/share/lib/mps:
	echo "target=$(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_JSS_JAR)"
	mkdir -p $@
	$(CP) $(SOURCE_RELEASE_XP_DIR)/$(SOURCE_RELEASE_XP_CLASSES_DIR)/$(IMPORT_JSS_JAR) $@/jss4.jar
ifdef JSS3_LOCATION
	$(CP) $(JSS3_LOCATION)/$(IMPORT_XPCLASS_JAR) $@/jss3.jar
endif

