# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This makefile appends to the variable OBJS the platform-dependent
# object modules that will be part of the nspr20 library.

CSRCS = \
	os2io.c      \
	os2sock.c    \
	os2thred.c   \
	os2cv.c      \
	os2gc.c      \
	os2misc.c    \
	os2inrval.c  \
	os2sem.c     \
	os2_errors.c \
	os2poll.c    \
	os2rng.c     \
	$(NULL)

ASFILES = os2emx.s os2vaclegacy.s

OBJS += $(addprefix md/os2/$(OBJDIR)/,$(CSRCS:.c=.$(OBJ_SUFFIX)))  \
	$(addprefix md/os2/$(OBJDIR)/,$(ASFILES:.$(ASM_SUFFIX)=.$(OBJ_SUFFIX)))

