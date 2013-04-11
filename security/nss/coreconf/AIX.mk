#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Config stuff for AIX.

include $(CORE_DEPTH)/coreconf/UNIX.mk

#
# There are two implementation strategies available on AIX:
# pthreads, and pthreads-user.  The default is pthreads.
# In both strategies, we need to use pthread_user.c, instead of
# aix.c.  The fact that aix.c is never used is somewhat strange.
# 
# So we need to do the following:
# - Default (PTHREADS_USER not defined in the environment or on
#   the command line):
#   Set PTHREADS_USER=1, USE_PTHREADS=1
# - PTHREADS_USER=1 set in the environment or on the command line:
#   Do nothing.
#
ifeq ($(PTHREADS_USER),1)
	USE_PTHREADS =            # just to be safe
	IMPL_STRATEGY = _PTH_USER
else
	USE_PTHREADS = 1
	PTHREADS_USER = 1
endif

DEFAULT_COMPILER = xlc_r

CC		= xlc_r
CCC		= xlC_r

CPU_ARCH	= rs6000

RANLIB		= ranlib

OS_CFLAGS	= -DAIX -DSYSV
OS_LIBS 	+= -blibpath:/usr/lib:/lib -lc -lm

DSO_LDOPTS	= -brtl -bnortllib -bM:SRE -bnoentry
MKSHLIB 	= $(LD) $(DSO_LDOPTS) -blibpath:/usr/lib:/lib -lc -lm

AIX_WRAP	= $(DIST)/lib/aixwrap.o
AIX_TMP		= $(OBJDIR)/_aix_tmp.o

ifdef MAPFILE
DSO_LDOPTS	+= -bexport:$(MAPFILE)
else
DSO_LDOPTS	+= -bexpall
endif

PROCESS_MAP_FILE = grep -v ';+' $< | grep -v ';-' | \
                sed -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,,' > $@

ifdef BUILD_OPT
	OPTIMIZER += -qmaxmem=-1
endif

ifeq ($(USE_64), 1)
	OS_CFLAGS	+= -DAIX_64BIT
	OBJECT_MODE=64
	export OBJECT_MODE
endif

