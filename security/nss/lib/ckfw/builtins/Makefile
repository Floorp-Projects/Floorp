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
MAKEFILE_CVS_ID = "@(#) $RCSfile: Makefile,v $ $Revision: 1.2 $ $Date: 2000/09/06 22:23:59 $ $Name:  $"

include manifest.mn
include config.mk
# can't do this in manifest.mn because OS_ARCH isn't defined there.
ifeq ($(OS_ARCH), WINNT)

EXTRA_LIBS = \
        $(DIST)/lib/nssckfw.lib \
        $(DIST)/lib/nssb.lib \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)plc4_s.lib \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)plds4_s.lib \
	wsock32.lib \
	winmm.lib \
	$(NULL)

else

# OSF 1 linker is very agressive. It includes the entire archive,
# not just the .o's that we need from that archive.
#
ifneq ($(OS_ARCH), OSF1)

EXTRA_LIBS += \
        $(DIST)/lib/libnssckfw.a \
        $(DIST)/lib/libnssb.a \
	$(DIST)/lib/libplc4.a \
	$(DIST)/lib/libplds4.a \
	$(NULL)

endif
endif



include $(CORE_DEPTH)/coreconf/config.mk
include $(CORE_DEPTH)/coreconf/rules.mk

certdata.c: certdata.txt certdata.perl
	perl certdata.perl < certdata.txt

# This'll need some help from a build person.


ifeq ($(OS_ARCH)$(OS_RELEASE), AIX4.1)
DSO_LDOPTS              = -bM:SRE -bh:4 -bnoentry
EXTRA_DSO_LDOPTS        = -lc
MKSHLIB                 = xlC $(DSO_LDOPTS)

$(SHARED_LIBRARY): $(OBJS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(MKSHLIB) -o $@ $(OBJS) $(EXTRA_LIBS) $(EXTRA_DSO_LDOPTS)
	chmod +x $@

endif

ifeq ($(OS_ARCH)$(OS_RELEASE), AIX4.2)
LD      += -G
endif 


