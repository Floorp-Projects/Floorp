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
# The Original Code is the Netscape Security Services for Java.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
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
#######################################################################
# Adjust specific variables for specific platforms                    #
#######################################################################

# We don't need static, import, or purify libraries
LIBRARY=
IMPORT_LIBRARY=
PURE_LIBRARY=

# Get rid of embedded "32" in library names on Windows
ifeq ($(OS_ARCH),WINNT)
SHARED_LIBRARY := $(subst 32,,$(SHARED_LIBRARY))
SHARED_LIBRARY_G := $(subst 32,,$(SHARED_LIBRARY_G))
endif

#######################################################################
# Adjust specific variables for all platforms                         #
#######################################################################

ifeq ($(OS_ARCH),WINNT)
LDOPTS += -PDB:NONE
endif

# Only used for "sanitizing" the release
STATIC_LIB_EXTENSION=
DYNAMIC_LIB_EXTENSION=
PURE_LIB_EXTENSION=

# Include "funky" link path to pick up ALL native libraries for OSF/1.
ifeq ($(OS_ARCH), OSF1)
	JAVA_LIBS += -L$(JAVA_HOME)/$(JAVA_LIBDIR).no
endif

LD_LIBS += -Wl,--whole-archive

#######################################################################
# Set the LD_LIBS value to encompass all static JSS, security, and    #
# dbm libraries                                                       #
#######################################################################

LD_LIBS += $(LIBJSSMANAGE) $(LIBJSSPKCS11) $(LIBJSSCRYPTO) $(LIBJSSUTIL) $(LIBJSSHCLHACKS) -Wl,--no-whole-archive $(LIBNSS) $(LIBCRYPTOHI) $(LIBCERTHI) $(LIBFORT) $(LIBNSSCKBI) $(LIBNSSB) $(LIBPK11WRAP) $(LIBJAR) $(LIBPKCS12) $(LIBPKCS7) $(LIBSECTOOL) $(LIBSMIME) $(LIBSOFTOKEN) $(LIBCERTDB) $(LIBFREEBL) $(LIBSECUTIL) $(LIBDBM)

#######################################################################
# Append additional LD_LIBS value to encompass all dynamic NSPR 2.0,  #
# java, and system libraries                                          #
#######################################################################
ifneq ($(STANDALONE_LIBJSS),1)
# NSPR is not included in libjss
ifeq ($(OS_ARCH), WINNT)
	LD_LIBS += $(DLLPLDS) $(DLLPLC) $(DLLPR) $(JAVA_LIBS) $(DLLSYSTEM)
else
	LD_LIBS += -L$(SOURCE_LIB_DIR) -lplds4 -lplc4 -lnspr4 $(JAVA_LIBS) $(DLLSYSTEM)
endif

else
# NSPR is included in libjss
ifeq ($(OS_ARCH), WINNT)
	LD_LIBS += $(LIBPLDS) $(LIBPLC) $(LIBPR) $(JAVA_LIBS) $(DLLSYSTEM)
else
	LD_LIBS += -L$(SOURCE_LIB_DIR) $(LIBPLDS) $(LIBPLC) $(LIBPR) $(JAVA_LIBS) $(DLLSYSTEM)
endif
endif
