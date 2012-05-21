#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

MODULES_LIBJAR_LCPPSRCS = \
		nsZipArchive.cpp \
		nsJARInputStream.cpp \
		nsJAR.cpp \
		nsJARFactory.cpp \
		nsJARProtocolHandler.cpp \
		nsJARChannel.cpp  \
		nsJARURI.cpp  \
		$(NULL)

MODULES_LIBJAR_LEXPORTS = \
		zipstruct.h \
		nsZipArchive.h \
		$(NULL)

MODULES_LIBJAR_LXPIDLSRCS = \
		nsIZipReader.idl \
		nsIJARChannel.idl \
		nsIJARURI.idl \
		nsIJARProtocolHandler.idl \
		$(NULL)

MODULES_LIBJAR_CPPSRCS := $(addprefix $(topsrcdir)/modules/libjar/, $(MODULES_LIBJAR_LCPPSRCS))

MODULES_LIBJAR_XPIDLSRCS := $(addprefix $(topsrcdir)/modules/libjar/, $(MODULES_LIBJAR_LXPIDLSRCS))

