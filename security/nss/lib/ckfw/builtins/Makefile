# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#
MAKEFILE_CVS_ID = "@(#) $RCSfile: Makefile,v $ $Revision: 1.12 $ $Date: 2003/04/20 04:23:24 $ $Name:  $"

include manifest.mn
include $(CORE_DEPTH)/coreconf/config.mk
include config.mk

EXTRA_LIBS = \
	$(DIST)/lib/$(LIB_PREFIX)nssckfw.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)nssb.$(LIB_SUFFIX) \
	$(NULL)

# can't do this in manifest.mn because OS_TARGET isn't defined there.
ifeq (,$(filter-out WIN%,$(OS_TARGET)))

# Link with the real NSPR DLLs for MinGW because the NSPR stubs in
# nsprstub.c can't resolve the references to the _imp__PR_XXX symbols.
# This is merely an expedient hack and not the right solution.
ifdef NS_USE_GCC
EXTRA_LIBS += \
	-L$(DIST)/lib \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	$(NULL)
else
EXTRA_LIBS += \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)plc4_s.lib \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)plds4_s.lib \
	$(NULL)
endif

else

EXTRA_LIBS += \
	$(DIST)/lib/$(LIB_PREFIX)plc4.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)plds4.$(LIB_SUFFIX) \
	$(NULL)

endif



include $(CORE_DEPTH)/coreconf/rules.mk

# Generate certdata.c.
generate:
	perl certdata.perl < certdata.txt

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


