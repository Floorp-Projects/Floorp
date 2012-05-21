#! gmake
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
DEPTH		= ../..
CORE_DEPTH	= ../..

VPATH		= $(CORE_DEPTH)/../dbm/tests

MODULE		= dbm

CSRCS		= lots.c

PROGRAM		= lots

include $(DEPTH)/coreconf/config.mk

include $(DEPTH)/dbm/config/config.mk

ifeq (,$(filter-out WIN%,$(OS_TARGET))) 
LIBDBM		= ../src/$(PLATFORM)/dbm$(STATIC_LIB_SUFFIX)
else
LIBDBM		= ../src/$(PLATFORM)/libdbm$(STATIC_LIB_SUFFIX)
endif

INCLUDES	+= -I$(CORE_DEPTH)/../dbm/include

LDFLAGS		= $(LDOPTS) $(LIBDBM)

include $(DEPTH)/coreconf/rules.mk

lots.pure: lots
	purify $(CC) -o lots.pure $(CFLAGS) $(OBJS) $(MYLIBS)

crash: crash.o $(MYLIBS)
	$(CC) -o crash $(CFLAGS) $^

crash.pure: crash.o $(MYLIBS)
	purify $(CC) -o crash.pure $(CFLAGS) $^

