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
# Initialize variables containing STATIC component library names      #
#######################################################################

#
# legacy library
#

LIBJDKCERTSEC10 = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)jdkcertsec10$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)

#
# jss hclhacks library
#

LIBJSSHCLHACKS = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)jsshclhacks$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)

#
# jss library
#

LIBJSS = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)jss$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)

#
# jss ssl jni library
#

LIBJSSSSL = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)jssssl$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)

#
# jss util jni library
#

LIBJSSUTIL = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)jssutil$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)

#
# jss pkcs #11 jni library
#

LIBJSSPKCS11= $(SOURCE_LIB_DIR)/$(LIB_PREFIX)jsspkcs11$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)

#
# jss pkcs #12 jni library
#

LIBJSSPKCS12= $(SOURCE_LIB_DIR)/$(LIB_PREFIX)jsspkcs12$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)

#
# jss crypto jni library
#

LIBJSSCRYPTO= $(SOURCE_LIB_DIR)/$(LIB_PREFIX)jsscrypto$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)

#
# jss manage jni library
#

LIBJSSMANAGE = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)jssmanage$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)

#
# jss policy library
#

LIBJSSPOLICY = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)jsspolicy$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)

#
# sectools library
#

LIBSECTOOLS  = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)sectools$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)

#
# sechlp library
#

LIBSECHLP  = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)sechlp$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)

#
# security libraries
#

LIBSSL     = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)ssl$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
LIBNSS     = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)nss$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
LIBPKCS7   = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)pkcs7$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
LIBPKCS12  = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)pkcs12$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
LIBCERT    = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)cert$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
LIBKEY     = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)key$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
LIBSECMOD  = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)secmod$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
LIBCRYPTO  = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)crypto$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
LIBSECUTIL = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)secutil$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
LIBJAR     = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)jar$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
LIBZLIB    = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)zlib$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
LIBHASH    = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)hash$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
LIBSECTOOL = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)sectool$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)

#
# DBM library
#

LIBDBM     = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)dbm$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)

#
# NSPR 2.0 libraries
#


ifeq ($(OS_ARCH),WINNT)
	ifeq ($(OS_TARGET),WIN95)
		LIBPLDS    = $(SOURCE_LIB_DIR)/plds3_s$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
		LIBPLC     = $(SOURCE_LIB_DIR)/plc3_s$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
		LIBPR      = $(SOURCE_LIB_DIR)/nspr3_s$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
	else
		LIBPLDS    = $(SOURCE_LIB_DIR)/libplds3_s$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
		LIBPLC     = $(SOURCE_LIB_DIR)/libplc3_s$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
		LIBPR      = $(SOURCE_LIB_DIR)/libnspr3_s$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
	endif
else
	LIBPLDS    = $(SOURCE_LIB_DIR)/libplds3$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
	LIBPLC     = $(SOURCE_LIB_DIR)/libplc3$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
	LIBPR      = $(SOURCE_LIB_DIR)/libnspr3$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)
endif

#
# svrcore libraries
#

	LIBSVRPLCY  = $(SOURCE_LIB_DIR)/$(LIB_PREFIX)svrplcy$(STATIC_LIB_EXTENSION)$(STATIC_LIB_SUFFIX_FOR_LINKING)


#######################################################################
# Initialize variables containing DYNAMIC component library names     #
#######################################################################

#
# legacy library
#

DLLJDKCERTSEC10 = $(SOURCE_LIB_DIR)/$(DLL_PREFIX)jdkcertsec10$(DYNAMIC_LIB_EXTENSION)$(JDK_DEBUG_SUFFIX)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)

#
# jss hclhacks library
#

DLLJSS = $(SOURCE_LIB_DIR)/$(DLL_PREFIX)jsshclhacks$(DYNAMIC_LIB_EXTENSION)$(JDK_DEBUG_SUFFIX)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)

#
# jss library
#

DLLJSS = $(SOURCE_LIB_DIR)/$(DLL_PREFIX)jss$(DYNAMIC_LIB_EXTENSION)$(JDK_DEBUG_SUFFIX)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)

#
# jssutil library
#

DLLJSSUTIL = $(SOURCE_LIB_DIR)/$(DLL_PREFIX)jssutil$(DYNAMIC_LIB_EXTENSION)$(JDK_DEBUG_SUFFIX)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)

#
# jsspkcs11 library
#

DLLJSSPKCS11 = $(SOURCE_LIB_DIR)/$(DLL_PREFIX)jsspkcs11$(DYNAMIC_LIB_EXTENSION)$(JDK_DEBUG_SUFFIX)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)

#
# jsspkcs12 library
#

DLLJSSPKCS12 = $(SOURCE_LIB_DIR)/$(DLL_PREFIX)jsspkcs12$(DYNAMIC_LIB_EXTENSION)$(JDK_DEBUG_SUFFIX)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)

#
# jsscrypto library
#

DLLJSSCRYPTO= $(SOURCE_LIB_DIR)/$(DLL_PREFIX)jsscrypto$(DYNAMIC_LIB_EXTENSION)$(JDK_DEBUG_SUFFIX)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)

#
# jssmanage library
#

DLLJSSMANAGE = $(SOURCE_LIB_DIR)/$(DLL_PREFIX)jssmanage$(DYNAMIC_LIB_EXTENSION)$(JDK_DEBUG_SUFFIX)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)

#
# jsspolicy library
#

DLLJSSPOLICY = $(SOURCE_LIB_DIR)/$(DLL_PREFIX)jsspolicy$(DYNAMIC_LIB_EXTENSION)$(JDK_DEBUG_SUFFIX)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)

#
# jssssl library
#

DLLJSSSSL = $(SOURCE_LIB_DIR)/$(DLL_PREFIX)jssssl$(DYNAMIC_LIB_EXTENSION)$(JDK_DEBUG_SUFFIX)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)

#
# secttools library
#

DLLSECTOOLS  = $(SOURCE_LIB_DIR)/$(DLL_PREFIX)sectools$(DYNAMIC_LIB_EXTENSION)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)

#
# sechlp library
#

DLLSECHLP  = $(SOURCE_LIB_DIR)/$(DLL_PREFIX)sechlp$(DYNAMIC_LIB_EXTENSION)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)

#
# there are NO dynamic security libraries
#

#
# DBM library
#

DLLDBM  = $(SOURCE_LIB_DIR)/$(DLL_PREFIX)dbm$(DYNAMIC_LIB_EXTENSION)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)

#
# NSPR 2.0 libraries
#

ifeq ($(OS_ARCH),WINNT)
	ifeq ($(OS_TARGET),WIN95)
		DLLPLDS    = $(SOURCE_LIB_DIR)/plds3$(DYNAMIC_LIB_EXTENSION)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)
		DLLPLC     = $(SOURCE_LIB_DIR)/plc3$(DYNAMIC_LIB_EXTENSION)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)
		DLLPR      = $(SOURCE_LIB_DIR)/nspr3$(DYNAMIC_LIB_EXTENSION)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)
	else
		DLLPLDS    = $(SOURCE_LIB_DIR)/libplds3$(DYNAMIC_LIB_EXTENSION)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)
		DLLPLC     = $(SOURCE_LIB_DIR)/libplc3$(DYNAMIC_LIB_EXTENSION)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)
		DLLPR      = $(SOURCE_LIB_DIR)/libnspr3$(DYNAMIC_LIB_EXTENSION)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)
	endif
else
	DLLPLDS    = $(SOURCE_LIB_DIR)/libplds3$(DYNAMIC_LIB_EXTENSION)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)
	DLLPLC     = $(SOURCE_LIB_DIR)/libplc3$(DYNAMIC_LIB_EXTENSION)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)
	DLLPR      = $(SOURCE_LIB_DIR)/libnspr3$(DYNAMIC_LIB_EXTENSION)$(DYNAMIC_LIB_SUFFIX_FOR_LINKING)
endif

#
# there are NO dynamic svrcore libraries
#


#######################################################################
# Tweak library names for windows and AIX.
#######################################################################

ifndef BUILD_OPT
	ifdef LIBRARY_NAME
		ifeq ($(OS_ARCH), WINNT)
			ifeq ($(OS_TARGET), WIN16)
				SHARED_LIBRARY_G = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION)16_g.dll
				IMPORT_LIBRARY_G = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION)16_g.lib
			else
				SHARED_LIBRARY_G = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION)32_g.dll
				IMPORT_LIBRARY_G = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION)32_g.lib
			endif
		else
			ifeq ($(OS_ARCH)$(OS_RELEASE), AIX4.1)
				SHARED_LIBRARY_G = $(OBJDIR)/lib$(LIBRARY_NAME)$(LIBRARY_VERSION)_shr_g.a
			else
				SHARED_LIBRARY_G = $(OBJDIR)/lib$(LIBRARY_NAME)$(LIBRARY_VERSION)_g.$(DLL_SUFFIX)
			endif
			ifdef HAVE_PURIFY
				ifdef DSO_BACKEND
					PURE_LIBRARY_G = $(OBJDIR)/purelib$(LIBRARY_NAME)$(LIBRARY_VERSION)_g.$(DLL_SUFFIX)
				endif
			endif
		endif
	endif
endif
