#
# The contents of this file are subject to the Netscape Public License
# Version 1.1 (the "NPL"); you may not use this file except in
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

#! gmake

MOD_DEPTH = ../../../..

include $(MOD_DEPTH)/config/config.mk

ifeq ($(OS_TARGET), WIN16)
CSRCS = \
	w16null.c \
	w16thred.c \
	w16proc.c \
    w16fmem.c \
    w16sock.c \
    w16mem.c \
    w16io.c  \
    w16gc.c  \
    w16error.c  \
    w16stdio.c  \
    w16callb.c \
    ntinrval.c \
    $(NULL)
else
ifeq ($(OS_TARGET), WIN95)
CSRCS =          \
    ntmisc.c \
    ntsem.c   \
    ntinrval.c \
    ntgc.c \
	w95thred.c \
	w95io.c \
	w95cv.c \
	w95sock.c \
	win32_errors.c \
    w32poll.c \
    w95dllmain.c \
    $(NULL)
else
CSRCS =          \
    ntdllmn.c \
    ntmisc.c \
    ntsem.c   \
    ntinrval.c \
    ntgc.c \
    ntthread.c \
    ntio.c    \
	win32_errors.c \
    w32poll.c \
    $(NULL)
endif
endif

TARGETS	= $(OBJS)

INCLUDES = -I$(DIST)/include/private -I$(DIST)/include

include $(MOD_DEPTH)/config/rules.mk

export:: $(TARGETS)

install:: export
