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

DEPTH		= ../..

ifdef STANDALONE_IMAGE_LIB
DIRS = png public src
EXTRA_LIBS = libzlib.a libpng.a
else
DIRS = classes public src

EXTRA_LIBS =

ifdef MOZ_NATIVE_PNG
EXTRA_LIBS	+= -lpng
else
DIRS		+= png
EXTRA_LIBS	+= libpng.a
endif

ifdef MOZ_NATIVE_ZLIB
EXTRA_LIBS	+= -lz
else
EXTRA_LIBS	+= libzlib.a
endif
endif

include $(DEPTH)/config/rules.mk
