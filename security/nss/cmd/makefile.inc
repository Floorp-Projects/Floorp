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

INCLUDES += \
	-I$(DEPTH)/security/lib/cert \
	-I$(DEPTH)/security/lib/key \
	-I$(DEPTH)/security/lib/util  \
	-I../include \
	$(NULL)


# For the time being, sec stuff is export only
# US_FLAGS = -DEXPORT_VERSION -DUS_VERSION

US_FLAGS = -DEXPORT_VERSION
EXPORT_FLAGS = -DEXPORT_VERSION

BASE_LIBS = \
	$(DIST)/lib/libdbm.a \
	$(DIST)/lib/libxp.a \
	$(DIST)/lib/libnspr21.a \
	$(NULL)


#There is a circular dependancy in security/lib, and here is a gross fix
SEC_LIBS = \
	$(DIST)/lib/libsecnav.a \
        $(DIST)/lib/libssl.a \
        $(DIST)/lib/libpkcs7.a \
        $(DIST)/lib/libcert.a \
        $(DIST)/lib/libkey.a \
	$(DIST)/lib/libsecmod.a \
        $(DIST)/lib/libcrypto.a \
        $(DIST)/lib/libsecutil.a \
        $(DIST)/lib/libssl.a \
        $(DIST)/lib/libpkcs7.a \
        $(DIST)/lib/libcert.a \
        $(DIST)/lib/libkey.a \
	$(DIST)/lib/libsecmod.a \
        $(DIST)/lib/libcrypto.a \
        $(DIST)/lib/libsecutil.a \
        $(DIST)/lib/libhash.a \
	$(NULL)

MYLIBDIR= ../lib/$(OBJDIR)
MYLIB	= $(MYLIBDIR)/libsectool.a

US_LIBS	= $(MYLIB) $(SEC_LIBS) $(BASE_LIBS) $(MYLIB) $(BASE_LIBS)
EX_LIBS	= $(MYLIB) $(SEC_LIBS) $(BASE_LIBS) $(MYLIB) $(BASE_LIBS) 

# this hack is necessary because rules.mk doesn't put anything like $(LIBS)
# on the link command line (!?!?!?!)
LDFLAGS += $(EX_LIBS)

