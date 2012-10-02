# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
MAKEFILE_CVS_ID = "@(#) $RCSfile: Makefile,v $ $Revision: 1.7 $ $Date: 2012/04/25 14:49:30 $"

include manifest.mn
include $(CORE_DEPTH)/coreconf/config.mk
include config.mk

EXTRA_LIBS = \
	$(DIST)/lib/$(LIB_PREFIX)nssckfw.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)nssb.$(LIB_SUFFIX) \
	$(NULL)

# can't do this in manifest.mn because OS_TARGET isn't defined there.
ifeq (,$(filter-out WIN%,$(OS_TARGET)))

ifdef NS_USE_GCC
EXTRA_LIBS += \
	-L$(NSPR_LIB_DIR) \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	-lcrypt32 \
	-ladvapi32 \
	-lrpcrt4 \
	$(NULL)
else 
EXTRA_SHARED_LIBS += \
        $(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)plc4.lib \
        $(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)plds4.lib \
        $(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)nspr4.lib \
        crypt32.lib \
	advapi32.lib \
	rpcrt4.lib \
        $(NULL)
endif # NS_USE_GCC
else

EXTRA_LIBS += \
	-L$(NSPR_LIB_DIR) \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	$(NULL)
endif


include $(CORE_DEPTH)/coreconf/rules.mk

# Generate certdata.c.
generate:
	$(PERL) certdata.perl < certdata.txt

# This'll need some help from a build person.


ifeq ($(OS_TARGET)$(OS_RELEASE), AIX4.1)
DSO_LDOPTS              = -bM:SRE -bh:4 -bnoentry
EXTRA_DSO_LDOPTS        = -lc
MKSHLIB                 = xlC $(DSO_LDOPTS)

$(SHARED_LIBRARY): $(OBJS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(MKSHLIB) -o $@ $(OBJS) $(EXTRA_LIBS) $(EXTRA_DSO_LDOPTS)
	chmod +x $@

endif

ifeq ($(OS_TARGET)$(OS_RELEASE), AIX4.2)
LD      += -G
endif 


