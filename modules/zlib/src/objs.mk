# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

MODULES_ZLIB_SRC_LCSRCS = \
		adler32.c \
		compress.c \
		crc32.c \
		deflate.c \
		gzclose.c \
		gzlib.c \
		gzread.c \
		gzwrite.c \
		infback.c \
		inffast.c \
		inflate.c \
		inftrees.c \
		trees.c \
		uncompr.c \
		zutil.c \
		$(NULL)

MODULES_ZLIB_SRC_CSRCS := $(addprefix $(topsrcdir)/modules/zlib/src/, $(MODULES_ZLIB_SRC_LCSRCS))

