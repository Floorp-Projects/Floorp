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
# Adjust specific variables for all platforms                         #
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

#######################################################################
# Set the LD_LIBS value to encompass all static JSS, security, and    #
# dbm libraries                                                       #
#######################################################################

LD_LIBS	+=	$(LIBJSSPKCS11)		\
		$(LIBJSSHCLHACKS)	\
		$(LIBJSSCRYPTO)		\
		$(LIBJSSUTIL)		\
		$(LIBJSSPOLICY)		\
		$(LIBSECMOD)		\
		$(LIBPKCS12)		\
		$(LIBPKCS7)		\
		$(LIBCERT)		\
		$(LIBKEY)		\
		$(LIBCRYPTO)		\
		$(LIBHASH)		\
		$(LIBSECUTIL)		\
		$(LIBDBM)		\
		$(NULL)

#######################################################################
# Append additional LD_LIBS value to encompass all dynamic NSPR 2.0,  #
# java, and system libraries                                          #
#######################################################################

ifeq ($(OS_ARCH), WINNT)
	LD_LIBS += $(DLLPLDS) $(DLLPLC) $(DLLPR) $(DLLSYSTEM)
else
	LD_LIBS += -L$(SOURCE_LIB_DIR) -lplds3 -lplc3 -lnspr3 $(DLLSYSTEM)
endif

