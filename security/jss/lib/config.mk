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
# The Original Code is the Netscape Security Services for Java.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-2000
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

LIBRARY =

SHARED_LIBRARY_LIBS=yes

SHARED_LIBRARY_DIRS = \
    ../org/mozilla/jss/crypto \
    ../org/mozilla/jss/SecretDecoderRing \
    ../org/mozilla/jss \
    ../org/mozilla/jss/pkcs11 \
    ../org/mozilla/jss/ssl \
    ../org/mozilla/jss/util \
    ../org/mozilla/jss/provider/java/security \
    $(NULL)

ifeq ($(OS_ARCH),WINNT)

SHARED_LIBRARY = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION).dll
IMPORT_LIBRARY = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION).lib

DLLFLAGS += -DEF:jss.def
RES = $(OBJDIR)/jss.res
RESNAME = jss.rc

EXTRA_SHARED_LIBS += \
    $(NSS_LIB_DIR)/nss3.lib \
    $(NSS_LIB_DIR)/smime3.lib \
    $(NSS_LIB_DIR)/ssl3.lib \
    $(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)plc4.lib \
    $(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)plds4.lib \
    $(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)nspr4.lib \
    $(JAVA_LIBS) \
    $(DLLSYSTEM) \
    $(NULL)

else

# Darwin needs to use the dylib_file linker option for nss to
# find its dependencies (libsoftokn3.dylib).
ifeq ($(OS_ARCH),Darwin)
    EXTRA_SHARED_LIBS += -dylib_file @executable_path/libsoftokn3.dylib:$(DIST)/lib/libsoftokn3.dylib
    DLL_SUFFIX = jnilib
endif

EXTRA_SHARED_LIBS += \
    -L$(NSS_LIB_DIR) \
    -lnss3 \
    -lsmime3 \
    -lssl3 \
    -L$(NSPR_LIB_DIR) \
    -lplc4 \
    -lplds4 \
    -lnspr4 \
    $(JAVA_LIBS) \
    $(NULL)

endif

# Include "funky" link path to pick up ALL native libraries for OSF/1.
ifeq ($(OS_ARCH), OSF1)
	JAVA_LIBS += -L$(JAVA_HOME)/$(JAVA_LIBDIR).no
endif

ifeq ($(OS_ARCH),Linux)
MAPFILE = $(OBJDIR)/jssmap.linux
ALL_TRASH += $(MAPFILE)
MKSHLIB += -Wl,--version-script,$(MAPFILE)
endif

ifeq ($(OS_ARCH),SunOS)
MAPFILE = $(OBJDIR)/jssmap.sun
ALL_TRASH += $(MAPFILE)
MKSHLIB += -M $(MAPFILE)
#ifndef USE_64
#ifeq ($(CPU_ARCH),sparc)
# The -R '$ORIGIN' linker option instructs libnss3.so to search for its
# dependencies (libfreebl_*.so) in the same directory where it resides.
#MKSHLIB += -R '$$ORIGIN'
#endif
#endif
endif

ifeq ($(OS_ARCH),AIX)
MAPFILE = $(OBJDIR)/jssmap.aix
ALL_TRASH += $(MAPFILE)
EXPORT_RULES = -bexport:$(MAPFILE)
endif

ifeq ($(OS_ARCH),HP-UX)
MAPFILE = $(OBJDIR)/jssmap.hp
ALL_TRASH += $(MAPFILE)
MKSHLIB += -c $(MAPFILE)
endif

ifeq ($(OS_ARCH), OSF1)
MAPFILE = $(OBJDIR)/jssmap.osf
ALL_TRASH += $(MAPFILE)
MKSHLIB += -hidden -input $(MAPFILE)
endif
