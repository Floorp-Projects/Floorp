#!gmake
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

DEPTH		= ../..

MODULE		= netcnvts
LIBRARY_NAME	= netcnvts

CSRCS		= \
	cvactive.c	\
	cvchunk.c	\
	cvcolor.c	\
	cvdisk.c	\
	cvextcon.c	\
	cvjscfg.c	\
	cvsimple.c	\
	cvunzip.c	\
	txview.c	\
	$(NULL)

ifndef MODULAR_NETLIB
CSRCS += \
	cvmime.c	\
	cvpics.c	\
	cvview.c	\
	$(NULL)
endif

EXPORTS= \
	cvactive.h	\
	cvview.h	\
	cvproxy.h	\
	cvextcon.h	\
	txview.h	\
    cvchunk.h   \
    cvcolor.h   \
    cvdisk.h    \
    cvsimple.h  \
    cvunzip.h   \
    cvjscfg.h

ifndef MODULAR_NETLIB
EXPORTS += \
    cvmime.h    \
    cvpics.h    \
    $(NULL)
endif

REQUIRES	= network img lay layer util parse pref js \
	security marimurl style zlib softupdt mimetype

include $(DEPTH)/config/rules.mk
