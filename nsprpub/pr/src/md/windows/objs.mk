#
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "MPL"); you may not use this file except in
# compliance with the MPL.  You may obtain a copy of the MPL at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the MPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
# for the specific language governing rights and limitations under the
# MPL.
# 
# The Initial Developer of this code under the MPL is Christopher
# Seawood <cls@seawood.org>.  Portions created by Christopher Seawood are
# Copyright (C) 1999 Christopher Seawood. All Rights Reserved.
#

ifeq (WINNT,$(MOZ_TARGET))
CSRCS = ntmisc.c \
	ntsem.c \
	ntinrval.c \
	ntgc.c \
	ntio.c \
	ntthread.c \
	ntdllmn.c \
	win32_errors.c \
	w32poll.c
else
ifeq (WIN95,$(MOZ_TARGET))
CSRCS =	ntmisc.c \
	ntsem.c \
	ntinrval.c \
	ntgc.c \
	w95thred.c \
	w95io.c \
	w95cv.c \
	w95sock.c \
	win32_errors.c \
	w32poll.c \
	w95dllmain.c
else
ifeq (WIN16,$(MOZ_TARGET))
CSRCS =	w16null.c \
	w16thred.c \
	w16proc.c \
	w16fmem.c \
	w16sock.c \
	w16mem.c \
	w16io.c \
	w16gc.c \
	w16error.c \
	w16stdio.c \
	w16callb.c \
	ntinrval.c
endif # win16
endif # win95
endif # winnt

CSRCS	+= $(PR_MD_CSRCS)
ASFILES += $(PR_MD_ASFILES)

OBJS += $(addprefix md/windows/$(OBJDIR)/,$(CSRCS:.c=.$(OBJ_SUFFIX)))  \
	$(addprefix md/windows/$(OBJDIR)/,$(ASFILES:.s=.$(OBJ_SUFFIX)))


