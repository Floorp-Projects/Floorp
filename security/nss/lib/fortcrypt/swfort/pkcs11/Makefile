#! gmake
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

include manifest.mn
include $(CORE_DEPTH)/coreconf/config.mk
include config.mk

CRYPTO_LIB = $(DIST)/lib/$(LIB_PREFIX)freebl.$(LIB_SUFFIX) 
#SWCILI    = ../$(OBJDIR)/$(LIB_PREFIX)swfci.$(LIB_SUFFIX)

# can't do this in manifest.mn because OS_TARGET isn't defined there.
ifeq (,$(filter-out WIN%,$(OS_TARGET)))

ifdef MOZILLA_SECURITY_BUILD
CRYPTO_LIB = $(DIST)/lib/crypto.lib
endif
ifdef MOZILLA_BSAFE_BUILD
CRYPTO_LIB += $(DIST)/lib/bsafe$(BSAFEVER).lib
CRYPTO_LIB += $(DIST)/lib/freebl.lib 
endif

# Link with the real NSPR DLLs for MinGW because the NSPR stubs in
# stub.c can't resolve the references to the _imp__PR_XXX symbols.
# This is merely an expedient hack and not the right solution.
ifdef NS_USE_GCC
EXTRA_LIBS = \
	$(DIST)/lib/$(LIB_PREFIX)swfci.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)softokn.$(LIB_SUFFIX) \
	$(CRYPTO_LIB) \
	$(DIST)/lib/$(LIB_PREFIX)secutil.$(LIB_SUFFIX) \
	-L$(DIST)/lib \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	-lwsock32 \
	-lwinmm \
	$(NULL)
else # ! NS_USE_GCC
EXTRA_LIBS = \
	$(DIST)/lib/swfci.lib \
	$(DIST)/lib/softokn.lib \
	$(CRYPTO_LIB) \
	$(DIST)/lib/secutil.lib \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)plc4_s.lib \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)plds4_s.lib \
	wsock32.lib \
	winmm.lib \
	$(NULL)
endif # NS_USE_GCC

else

ifdef MOZILLA_SECURITY_BUILD
CRYPTO_LIB = $(DIST)/lib/$(LIB_PREFIX)crypto.$(LIB_SUFFIX)
endif
ifdef MOZILLA_BSAFE_BUILD
CRYPTO_LIB += $(DIST)/lib/$(LIB_PREFIX)bsafe.$(LIB_SUFFIX)
CRYPTO_LIB += $(DIST)/lib/$(LIB_PREFIX)freebl.$(LIB_SUFFIX)
endif

EXTRA_LIBS += \
	$(DIST)/lib/$(LIB_PREFIX)swfci.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)softokn.$(LIB_SUFFIX) \
	$(CRYPTO_LIB) \
	$(DIST)/lib/$(LIB_PREFIX)secutil.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)plc4.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)plds4.$(LIB_SUFFIX) \
	$(NULL)

endif

INST_JS                 = inst.js
LIBCI_JAR               = $(OBJDIR)/lib$(LIBRARY_NAME).jar
LIBCI_JAR_SRC           = $(INST_JS) pk11inst $(SHARED_LIBRARY)

ifneq ($(OS_TARGET), WIN16)
TARGETS : $(LIBCI_JAR)
endif

ifeq ($(OS_TARGET), WIN16)
# note that rules.mk is not included below for WIN16
all:
	@echo Skipping fortcrypt directory for 16-bit windows builds

all_platforms alltags clean clobber clobber_all realclean: all

boot export install libs program release: all

endif

#$(SHARED_LIBRARY): $(SWCILIB)

#
# The following rules packages the shared library into a JAR,
# ready to be signed
#
$(OBJDIR)/replace: replace.c
	$(CC) -o $@ $<

# ZIP options:
# -5 means medium compression
# -q means quiet
# -j means do not store tree structure, all files go into one dir
#
$(LIBCI_JAR): $(LIBCI_JAR_SRC)
	@echo +++ building $@ from $(LIBCI_JAR_SRC)
	@rm -f $@
	zip -5qj $@ $(LIBCI_JAR_SRC) 

$(LIBSWCI_JAR): $(LIBSWCI_JAR_SRC)
	@echo +++ building $@ from $(LIBSWCI_JAR_SRC)
	@rm -f $@
	zip -5qj $@ $(LIBSWCI_JAR_SRC) 


MD_FILES += $(LIBCI_JAR) $(LIBSWCI_JAR)

# coreconf doesn't build the AIX shared library for FORTEZZA,
# so I'm going to override their shared library command and build the shared
# library the way config used to.
#


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

ifneq ($(OS_TARGET), WIN16)
include $(CORE_DEPTH)/coreconf/rules.mk
endif

