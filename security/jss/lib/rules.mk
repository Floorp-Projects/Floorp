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

release_md::  release_sanitize

release_sanitize::
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(DLL_PREFIX)jsscrypto$(DYNAMIC_LIB_EXTENSION)$(DYNAMIC_LIB_SUFFIX)
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(DLL_PREFIX)jsshclhacks$(DYNAMIC_LIB_EXTENSION)$(DYNAMIC_LIB_SUFFIX)
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(DLL_PREFIX)jssmanage$(DYNAMIC_LIB_EXTENSION)$(DYNAMIC_LIB_SUFFIX)
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(DLL_PREFIX)jsspkcs11$(DYNAMIC_LIB_EXTENSION)$(DYNAMIC_LIB_SUFFIX)
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(DLL_PREFIX)jsspolicy$(DYNAMIC_LIB_EXTENSION)$(DYNAMIC_LIB_SUFFIX)
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(DLL_PREFIX)jssssl$(DYNAMIC_LIB_EXTENSION)$(DYNAMIC_LIB_SUFFIX)
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(DLL_PREFIX)jssutil$(DYNAMIC_LIB_EXTENSION)$(DYNAMIC_LIB_SUFFIX)
ifeq ($(OS_ARCH),WINNT)
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(IMPORT_LIB_PREFIX)jsscrypto$(IMPORT_LIB_EXTENSION)$(IMPORT_LIB_SUFFIX)
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(IMPORT_LIB_PREFIX)jsshclhacks$(IMPORT_LIB_EXTENSION)$(IMPORT_LIB_SUFFIX)
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(IMPORT_LIB_PREFIX)jssmanage$(IMPORT_LIB_EXTENSION)$(IMPORT_LIB_SUFFIX)
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(IMPORT_LIB_PREFIX)jsspkcs11$(IMPORT_LIB_EXTENSION)$(IMPORT_LIB_SUFFIX)
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(IMPORT_LIB_PREFIX)jsspolicy$(IMPORT_LIB_EXTENSION)$(IMPORT_LIB_SUFFIX)
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(IMPORT_LIB_PREFIX)jssssl$(IMPORT_LIB_EXTENSION)$(IMPORT_LIB_SUFFIX)
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(IMPORT_LIB_PREFIX)jssutil$(IMPORT_LIB_EXTENSION)$(IMPORT_LIB_SUFFIX)
else
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(PURE_LIB_PREFIX)jsscrypto$(PURE_LIB_EXTENSION)$(PURE_LIB_SUFFIX)
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(PURE_LIB_PREFIX)jsshclhacks$(PURE_LIB_EXTENSION)$(PURE_LIB_SUFFIX)
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(PURE_LIB_PREFIX)jssmanage$(PURE_LIB_EXTENSION)$(PURE_LIB_SUFFIX)
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(PURE_LIB_PREFIX)jsspkcs11$(PURE_LIB_EXTENSION)$(PURE_LIB_SUFFIX)
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(PURE_LIB_PREFIX)jsspolicy$(PURE_LIB_EXTENSION)$(PURE_LIB_SUFFIX)
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(PURE_LIB_PREFIX)jssssl$(PURE_LIB_EXTENSION)$(PURE_LIB_SUFFIX)
	-rm $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)/$(PURE_LIB_PREFIX)jssutil$(PURE_LIB_EXTENSION)$(PURE_LIB_SUFFIX)
endif
