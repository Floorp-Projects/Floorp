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
ifndef MOZ_JAVA
LOCAL_JMC_SUBDIR = .
endif

DEPTH		= ../../..

MODULE		= img
LIBRARY_NAME	= img

CSRCS		= color.c \
		  colormap.c \
		  dither.c \
		  dummy_nc.c \
		  external.c \
		  gif.c \
		  if.c \
		  ilclient.c \
		  il_util.c \
		  jpeg.c \
		  scale.c \
		  xbm.c \
		  ipng.c \
		  png_png.c \
		  $(NULL)

EXPORTS		= PIMGCB.h PPSIMGCB.h

REQUIRES	= jtools java zlib nspr dbm jpeg util img png layer js

LOCAL_INCLUDES	= -I../png -I. -I$(DEPTH)/dist/public/zlib

EXTRA_LIBS	= $(DIST)/lib/libzlib.a $(DIST)/lib/libpng.a

#
# Generate MIMGCB.c (and similar sources) here.
# MIMGCB.h is generated in ../public/_jmc/
#
JMC_GEN		= MIMGCB MPSIMGCB
JMC_GEN_FLAGS	= -cimpl -noexc -module

include $(DEPTH)/config/rules.mk
