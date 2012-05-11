# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This makefile appends to the variable OBJS the platform-dependent
# object modules that will be part of the nspr20 library.

CSRCS =          \
	unix.c    \
	unix_errors.c \
	uxproces.c \
	uxrng.c \
	uxshm.c \
	uxwrap.c \
	$(NULL)

ifneq ($(USE_PTHREADS),1)
CSRCS += uxpoll.c
endif

ifeq ($(PTHREADS_USER),1)
CSRCS += pthreads_user.c
endif

CSRCS	+= $(PR_MD_CSRCS)
ASFILES += $(PR_MD_ASFILES)

OBJS += $(addprefix md/unix/$(OBJDIR)/,$(CSRCS:.c=.$(OBJ_SUFFIX)))  \
	$(addprefix md/unix/$(OBJDIR)/,$(ASFILES:.s=.$(OBJ_SUFFIX)))

