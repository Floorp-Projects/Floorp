#! gmake
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#
DEPTH		= ../..
CORE_DEPTH	= ../..

VPATH		= $(CORE_DEPTH)/../dbm/tests

MODULE		= dbm

CSRCS		= lots.c

PROGRAM		= lots

include $(DEPTH)/coreconf/config.mk

ifeq ($(OS_ARCH),WINNT)
DEFINES		+= -DSTDARG -DSTDC_HEADERS
LIBDBM		= ../src/$(PLATFORM)/dbm$(STATIC_LIB_SUFFIX)
else
LIBDBM		= ../src/$(PLATFORM)/libdbm$(STATIC_LIB_SUFFIX)
endif

ifeq ($(OS_ARCH),AIX)
CFLAGS		+= -DSTDARG
endif

ifeq ($(OS_ARCH),BSD_386)
CFLAGS		+= -g -I../../../include -DXP_UNIX -g -DBSDI -DHAVE_STRERROR -D__386BSD__ -DDEBUG -DMEMMOVE -D__DBINTERFACE_PRIVATE 
endif

ifeq ($(OS_ARCH),FreeBSD)
CFLAGS		+= -DSTDARG
endif

ifeq ($(OS_ARCH),HP-UX)
CFLAGS		+= -DSTDARG
endif

ifeq ($(OS_ARCH),IRIX)
CFLAGS		+= -g -I../../../include -DDEBUG -DSTDARG
endif

ifeq ($(OS_ARCH),OSF1)
CFLAGS		+= -DSTDARG
endif

ifeq ($(OS_ARCH),Linux)
CFLAGS		+= -DSTDARG
endif

ifeq ($(OS_ARCH),NCR)
CFLAGS		+= -DSTDARG
endif

ifeq ($(OS_ARCH),SCO_SV)
CFLAGS		+= -DSTDARG
endif

ifeq ($(OS_ARCH),SunOS)
CFLAGS		+= -g -I../../../include -D_sun_
endif

ifeq ($(OS_ARCH),UNIXWARE)
CFLAGS		+= -DSTDARG
endif

INCLUDES	+= -I../include
INCLUDES	+= -I$(CORE_DEPTH)/../dbm/include

LDFLAGS		= $(LDOPTS) $(LIBDBM)

include $(DEPTH)/coreconf/rules.mk

lots.pure: lots
	purify $(CC) -o lots.pure $(CFLAGS) $(OBJS) $(MYLIBS)

crash: crash.o $(MYLIBS)
	$(CC) -o crash $(CFLAGS) $^

crash.pure: crash.o $(MYLIBS)
	purify $(CC) -o crash.pure $(CFLAGS) $^



DBM_SRCS = $(CSRCS)

export:: $(DBM_SRCS)

libs:: $(DBM_SRCS)

program:: $(DBM_SRCS)

private_export:: $(DBM_SRCS)

