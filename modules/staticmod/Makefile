# Generated automatically from Makefile.in by configure.
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#

DEPTH		= ../..
topsrcdir	= ../..
srcdir		= .

include $(DEPTH)/config/autoconf.mk

MODULE		= staticmod
LIBRARY_NAME	= staticmod
SHORT_LIBNAME	= statcmod
IS_COMPONENT	= 1

CPPSRCS		= nsStaticModule.cpp

LOCAL_INCLUDES	= -I$(srcdir)

SHARED_LIBRARY_LIBS = \
		$(foreach m, $(MOZ_STATIC_COMPONENTS), $(DIST)/lib/components/lib$(m).$(LIB_SUFFIX))

include $(topsrcdir)/config/rules.mk

EXTRA_DSO_LDOPTS += \
		$(ZLIB_LIBS) \
		$(XPCOM_LIBS) \
		$(MOZ_JS_LIBS) \
		$(NSPR_LIBS) \
		$(PNG_LIBS) \
		-ljsdom

GARBAGE += nsStaticModule.cpp

list:
	echo $(MOZ_STATIC_COMPONENTS)

nsStaticModule.cpp: nsStaticModule.cpp.in Makefile Makefile.in $(DEPTH)/config/autoconf.mk
	rm -f $@
	cat $< | sed -e "s|@COMPONENT_NS_GET_MODULE@|$(foreach m, $(MOZ_STATIC_COMPONENTS), REGISTER_MODULE_USING($(m)_NSGetModule);)|" | \
	sed -e "s|@COMPONENT_LIST@|$(foreach m, $(MOZ_STATIC_COMPONENTS), { $(m)_NSGM_comps, $(m)_NSGM_comp_count }, )|" | \
	sed -e "s|@DECLARE_COMPONENT_LIST@|$(foreach m,$(MOZ_STATIC_COMPONENTS), extern \"C\" nsresult $(m)_NSGetModule(nsIComponentManager *servMgr, nsIFile* aPath, nsIModule** return_cobj); extern nsModuleComponentInfo*  $(m)_NSGM_comps; extern PRUint32 $(m)_NSGM_comp_count;)|" > $@









