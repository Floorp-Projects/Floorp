#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.
#

DEPTH		= ../../..

MODULE		= zlib
LIBRARY_NAME	= zlib

REQUIRES	= nspr java

LCFLAGS		=

CSRCS		= adler32.c \
		  crc32.c \
		  compress.c \
		  uncompr.c \
		  deflate.c \
		  trees.c \
		  zutil.c \
		  inflate.c \
		  infblock.c \
		  inftrees.c \
		  infcodes.c \
		  infutil.c \
		  inffast.c \
		  $(NULL)

# XXX JDK_GEN should add the _gen/* exports
EXPORTS		= zlib.h zconf.h

include $(DEPTH)/config/rules.mk

