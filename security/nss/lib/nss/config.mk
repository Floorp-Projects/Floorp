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

#
#  Override TARGETS variable so that only static libraries
#  are specifed as dependencies within rules.mk.
#

#TARGETS        = $(LIBRARY)
#SHARED_LIBRARY =
#IMPORT_LIBRARY =
#PROGRAM        =

# can't do this in manifest.mn because OS_ARCH isn't defined there.
ifeq ($(OS_ARCH), WINNT)

# don't want the 32 in the shared library name
SHARED_LIBRARY = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION).dll
IMPORT_LIBRARY = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION).lib

RES = $(OBJDIR)/$(LIBRARY_NAME).res
RESNAME = $(LIBRARY_NAME).rc

# $(PROGRAM) has explicit dependencies on $(EXTRA_LIBS)
SHARED_LIBRARY_LIBS = \
	$(DIST)/lib/certhi.lib \
	$(DIST)/lib/cryptohi.lib \
	$(DIST)/lib/pk11wrap.lib \
	$(DIST)/lib/certdb.lib \
	$(DIST)/lib/secutil.lib \
	$(DIST)/lib/nsspki.lib \
	$(DIST)/lib/nssdev.lib \
	$(DIST)/lib/nssb.lib \
	$(NULL)

SHARED_LIBRARY_DIRS = \
	../certhigh \
	../cryptohi \
	../pk11wrap \
	../certdb \
	../util \
	../pki \
	../dev \
	../base \
	$(NULL)

EXTRA_SHARED_LIBS += \
	$(DIST)/lib/softokn3.lib \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)plc4.lib \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)plds4.lib \
	$(DIST)/lib/$(NSPR31_LIB_PREFIX)nspr4.lib \
	$(NULL)
else

# $(PROGRAM) has explicit dependencies on $(EXTRA_LIBS)
SHARED_LIBRARY_LIBS = \
	$(DIST)/lib/libcerthi.$(LIB_SUFFIX) \
	$(DIST)/lib/libpk11wrap.$(LIB_SUFFIX) \
	$(DIST)/lib/libcryptohi.$(LIB_SUFFIX) \
	$(DIST)/lib/libcertdb.$(LIB_SUFFIX) \
	$(DIST)/lib/libsecutil.$(LIB_SUFFIX) \
	$(DIST)/lib/libnsspki.$(LIB_SUFFIX) \
	$(DIST)/lib/libnssdev.$(LIB_SUFFIX) \
	$(DIST)/lib/libnssb.$(LIB_SUFFIX) \
	$(NULL)

SHARED_LIBRARY_DIRS = \
	../certhigh \
	../pk11wrap \
	../cryptohi \
	../certdb \
	../util \
	../pki \
	../dev \
	../base \
	$(NULL)

# $(PROGRAM) has NO explicit dependencies on $(EXTRA_SHARED_LIBS)
# $(EXTRA_SHARED_LIBS) come before $(OS_LIBS), except on AIX.
EXTRA_SHARED_LIBS += \
	-L$(DIST)/lib/ \
	-lsoftokn3 \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	$(NULL)
endif

