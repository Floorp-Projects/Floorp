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

MODULES_ZLIB_SRC_LCSRCS = \
		adler32.c \
		compress.c \
		crc32.c \
		deflate.c \
		gzio.c \
		infback.c \
		inffast.c \
		inflate.c \
		inftrees.c \
		trees.c \
		uncompr.c \
		zutil.c \
		$(NULL)

MODULES_ZLIB_SRC_LEXPORTS = zlib.h zconf.h mozzconf.h

MODULES_ZLIB_SRC_CSRCS := $(addprefix $(topsrcdir)/modules/zlib/src/, $(MODULES_ZLIB_SRC_LCSRCS))
MODULES_ZLIB_SRC_EXPORTS := $(addprefix $(topsrcdir)/modules/zlib/src/, $(MODULES_ZLIB_SRC_LEXPORTS))

