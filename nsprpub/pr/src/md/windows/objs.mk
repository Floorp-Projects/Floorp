# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


ifeq ($(OS_TARGET),WINNT)
CSRCS = ntmisc.c \
	ntsec.c \
	ntsem.c \
	ntinrval.c \
	ntgc.c \
	ntio.c \
	ntthread.c \
	ntdllmn.c \
	win32_errors.c \
	w32ipcsem.c \
	w32poll.c \
	w32rng.c \
	w32shm.c
else
ifeq (,$(filter-out WIN95 WINCE WINMO, $(OS_TARGET)))
CSRCS =	ntmisc.c \
	ntsec.c \
	ntsem.c \
	ntinrval.c \
	ntgc.c \
	w95thred.c \
	w95io.c \
	w95cv.c \
	w95sock.c \
	win32_errors.c \
	w32ipcsem.c \
	w32poll.c \
	w32rng.c \
	w32shm.c \
	w95dllmain.c
else
endif # win95
endif # winnt

CSRCS	+= $(PR_MD_CSRCS)
ASFILES += $(PR_MD_ASFILES)

OBJS += $(addprefix md/windows/$(OBJDIR)/,$(CSRCS:.c=.$(OBJ_SUFFIX)))  \
	$(addprefix md/windows/$(OBJDIR)/,$(ASFILES:.s=.$(OBJ_SUFFIX)))


