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
# Copyright (C) 2000 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#

MODULES_LIBJAR_LCPPSRCS = \
		nsJARInputStream.cpp \
		nsZipArchive.cpp \
		nsJAR.cpp \
		nsJARFactory.cpp \
		nsWildCard.cpp \
		$(NULL)

MODULES_LIBJAR_LEXPORTS = \
		zipfile.h \
		zipstub.h \
		$(NULL)

MODULES_LIBJAR_LXPIDLSRCS = \
		nsIZipReader.idl \
		$(NULL)

MODULES_LIBJAR_CPPSRCS := $(addprefix $(topsrcdir)/modules/libjar/, $(MODULES_LIBJAR_LCPPSRCS))

MODULES_LIBJAR_XPIDLSRCS := $(addprefix $(topsrcdir)/modules/libjar/, $(MODULES_LIBJAR_LXPIDLSRCS))

