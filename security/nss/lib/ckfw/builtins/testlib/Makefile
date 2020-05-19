#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include manifest.mn

include $(CORE_DEPTH)/coreconf/config.mk

ifdef BUILD_IDG
DEFINES += -DNSSDEBUG
endif

# Needed for compilation of $(OBJDIR)/certdata.c
INCLUDES += -I.

#
# To create a loadable module on Darwin, we must use -bundle.
#
ifeq ($(OS_TARGET),Darwin)
DSO_LDOPTS = -bundle
endif

ifdef USE_GCOV
DSO_LDOPTS += --coverage
endif

EXTRA_LIBS =                                       \
	$(DIST)/lib/$(LIB_PREFIX)nssckfw.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)nssb.$(LIB_SUFFIX)    \
	$(NULL)

# If the OS_TARGET is WIN%, the path of shared libs could be different.
ifeq (,$(filter-out WIN%,$(OS_TARGET)))
# If using GCC, just inform the name of the libs.
ifdef NS_USE_GCC
EXTRA_SHARED_LIBS +=  \
	-L$(NSPR_LIB_DIR) \
	-lplc4            \
	-lplds4           \
	-lnspr4           \
	$(NULL)
else # NS_USE_GCC - If not using GCC, inform the absolute path.
EXTRA_SHARED_LIBS += 				\
	$(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)plc4.lib  \
	$(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)plds4.lib \
	$(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)nspr4.lib \
	$(NULL)
endif # NS_USE_GCC
else # OS_TARGET != WIN
EXTRA_SHARED_LIBS +=  \
	-L$(NSPR_LIB_DIR) \
	-lplc4            \
	-lplds4           \
	-lnspr4           \
	$(NULL)
endif # OS_TARGET

SIMPLE_OBJS   = $(notdir $(CSRCS:.c=$(OBJ_SUFFIX)))
OBJS          = $(addprefix $(OBJDIR)/$(PROG_PREFIX), $(SIMPLE_OBJS))
vpath %c $(dir $(CSRCS)) # $(OBJDIR)


include $(CORE_DEPTH)/coreconf/rules.mk

CFLAGS += -I$(CORE_DEPTH)/lib/ckfw/builtins

# Generate certdata-testlib.c.
ifndef NSS_CERTDATA-TESTLIB_TXT
NSS_CERTDATA-TESTLIB_TXT = certdata-testlib.txt
endif

$(OBJDIR)/certdata-testlib.c: $(NSS_CERTDATA-TESTLIB_TXT) | $$(@D)/d
	$(PERL) ../certdata.perl $(NSS_CERTDATA-TESTLIB_TXT) $@

