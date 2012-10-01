#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(CORE_DEPTH)/coreconf/UNIX.mk

USE_PTHREADS = 1

ifeq ($(USE_PTHREADS),1)
	IMPL_STRATEGY = _PTH
endif

CC			= qcc
CCC			= qcc
RANLIB			= ranlib

DEFAULT_COMPILER = qcc
ifeq ($(OS_TEST),mips)
	CPU_ARCH	= mips
else
	CPU_ARCH	= x86
endif

MKSHLIB		= $(CC) -shared -Wl,-soname -Wl,$(@:$(OBJDIR)/%.so=%.so)
ifdef BUILD_OPT
	OPTIMIZER	= -O2
endif

OS_CFLAGS		= $(DSO_CFLAGS) $(OS_REL_CFLAGS) -Vgcc_ntox86 -Wall -pipe -DNTO -DHAVE_STRERROR -D_QNX_SOURCE -D_POSIX_C_SOURCE=199506 -D_XOPEN_SOURCE=500

ifdef USE_PTHREADS
	DEFINES		+= -D_REENTRANT
endif

ARCH			= QNX

DSO_CFLAGS		= -Wc,-fPIC
DSO_LDOPTS		= -shared
