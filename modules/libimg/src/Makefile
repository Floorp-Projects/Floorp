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

ifdef STANDALONE_IMAGE_LIB

CPPSRCS		= ilNetReader.cpp \
		  xpcompat.cpp \
		  color.cpp \
		  colormap.cpp \
		  dither.cpp \
		  gif.cpp \
		  if.cpp \
		  ilclient.cpp \
		  il_util.cpp \
		  jpeg.cpp \
		  scale.cpp \
		  xbm.cpp \
		  ipng.cpp \
		  png_png.cpp \
		  $(NULL)

EXPORTS		= PIMGCB.h PPSIMGCB.h

REQUIRES	= zlib nspr jpeg util img png xpcom

LOCAL_INCLUDES	= -I../png -I. -I$(DEPTH)/dist/public/zlib

EXTRA_LIBS	= $(DIST)/lib/libzlib.a $(DIST)/lib/libpng.a $(DIST)/lib/libnspr21.a $(dist)/lib/libjpeg.a $(DIST)/lib/libplc21.a $(DIST)/lib/libutil.a

symbols::
	@echo "LIBRARY_NAME     is $(LIBRARY_NAME)"
	@echo "LIBRARY          is $(LIBRARY)"
	@echo  "DIST		is $(DIST)"

else
CPPSRCS		= color.cpp \
		  colormap.cpp \
		  dither.cpp \
		  dummy_nc.cpp \
		  external.cpp \
		  gif.cpp \
		  if.cpp \
		  ilclient.cpp \
		  il_util.cpp \
		  jpeg.cpp \
		  scale.cpp \
		  xbm.cpp \
		  ipng.cpp \
		  png_png.cpp \
		  ilNetReader.cpp \
		  $(NULL)

EXPORTS		= PIMGCB.h PPSIMGCB.h

REQUIRES	= jtools java zlib nspr dbm jpeg util img png layer js xpcom

LOCAL_INCLUDES	= -I../png -I. -I$(DEPTH)/dist/public/zlib

EXTRA_LIBS	= $(DIST)/lib/libzlib.a $(DIST)/lib/libpng.a

#
# Generate MIMGCB.c (and similar sources) here.
# MIMGCB.h is generated in ../public/_jmc/
#
JMC_GEN		= MIMGCB MPSIMGCB
JMC_GEN_FLAGS	= -cimpl -noexc -module
endif

include $(DEPTH)/config/rules.mk

#
# This is about as far from the right way of doing things as you can get.
# HACK larubbio fix me
#
ifdef STANDALONE_IMAGE_LIB
CC = $(CCC)
endif
