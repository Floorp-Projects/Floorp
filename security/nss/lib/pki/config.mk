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

#CONFIG_CVS_ID = "@(#) $RCSfile: config.mk,v $ $Revision: 1.3 $ $Date: 2002/02/15 22:53:57 $ $Name:  $"

ifdef BUILD_IDG
DEFINES += -DNSSDEBUG
endif

# can't do this in manifest.mn because OS_TARGET isn't defined there.
ifeq (,$(filter-out WIN%,$(OS_TARGET)))

# don't want the 32 in the shared library name
SHARED_LIBRARY = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION).dll
IMPORT_LIBRARY = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION).lib

DLLFLAGS += -DEF:nsspki.def
RES = $(OBJDIR)/nsspki.res
RESNAME = nsspki.rc

# $(PROGRAM) has explicit dependencies on $(EXTRA_LIBS)

SHARED_LIBRARY_LIBS = \
	$(DIST)/lib/nssb.lib \
	$(DIST)/lib/nssdev.lib \
	$(DIST)/lib/nsspki.lib \
	$(NULL)

SHARED_LIBRARY_DIRS = \
	../base \
	../dev \
	$(NULL)

EXTRA_LIBS += \
	$(NULL)

EXTRA_SHARED_LIBS += \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)plc4.lib \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)plds4.lib \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)nspr4.lib \
	$(NULL)

# $(PROGRAM) has NO explicit dependencies on $(OS_LIBS)
#OS_LIBS += \
#	wsock32.lib \
#	winmm.lib \
#	$(NULL)
else

# $(PROGRAM) has explicit dependencies on $(EXTRA_LIBS)
SHARED_LIBRARY_LIBS = \
	$(DIST)/lib/libnssb.$(LIB_SUFFIX) \
	$(DIST)/lib/libnssdev.$(LIB_SUFFIX) \
	$(DIST)/lib/libnsspki.$(LIB_SUFFIX) \
	$(NULL)

EXTRA_LIBS += \
	$(NULL)

SHARED_LIBRARY_DIRS = \
	../base \
	../dev \
	$(NULL)

# $(PROGRAM) has NO explicit dependencies on $(EXTRA_SHARED_LIBS)
# $(EXTRA_SHARED_LIBS) come before $(OS_LIBS), except on AIX.
EXTRA_SHARED_LIBS += \
	-L$(DIST)/lib/ \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	$(NULL)
endif

ifeq ($(OS_TARGET),SunOS)
MAPFILE = $(OBJDIR)/nsspkimap.sun
ALL_TRASH += $(MAPFILE)
MKSHLIB += -M $(MAPFILE)
ifndef USE_64
ifeq ($(CPU_ARCH),sparc)
# The -R '$ORIGIN' linker option instructs libnss3.so to search for its
# dependencies (libfreebl_*.so) in the same directory where it resides.
MKSHLIB += -R '$$ORIGIN'
endif
endif
endif

ifeq ($(OS_TARGET),AIX)
MAPFILE = $(OBJDIR)/nsspkimap.aix
ALL_TRASH += $(MAPFILE)
EXPORT_RULES = -bexport:$(MAPFILE)
endif

ifeq ($(OS_TARGET),HP-UX)
MAPFILE = $(OBJDIR)/nsspkimap.hp
ALL_TRASH += $(MAPFILE)
MKSHLIB += -c $(MAPFILE)
endif

ifeq ($(OS_TARGET), OSF1)
MAPFILE = $(OBJDIR)/nsspkimap.osf
ALL_TRASH += $(MAPFILE)
MKSHLIB += -hidden -input $(MAPFILE)
endif

ifeq ($(OS_TARGET),Linux)
MAPFILE = $(OBJDIR)/nsspkimap.linux
ALL_TRASH += $(MAPFILE)
MKSHLIB += -Wl,--version-script,$(MAPFILE)
endif


	

