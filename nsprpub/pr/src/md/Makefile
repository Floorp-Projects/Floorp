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

MOD_DEPTH = ../../..

include $(MOD_DEPTH)/config/config.mk

ifeq ($(OS_ARCH),WINNT)
  ifeq ($(OS_TARGET),OS2)
    DIRS = os2
  else
    DIRS = windows
  endif
else
  ifeq ($(OS_ARCH),BeOS)
    DIRS = beos
  else
    DIRS = unix
  endif
endif

# Disable optimization of the nspr on SunOS4.1.3
ifeq ($(OS_ARCH),SunOS)
ifeq ($(OS_RELEASE),4.1.3_U1)
OPTIMIZER =
endif
endif

CSRCS =          \
	prosdep.c \
	$(NULL)

TARGETS	= $(OBJS)

INCLUDES = -I$(DIST)/include/private -I$(DIST)/include

include $(MOD_DEPTH)/config/rules.mk

export:: $(TARGETS)

install:: export
