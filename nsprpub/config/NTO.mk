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
# Copyright (C) 1999 Netscape Communications Corporation.  All Rights
# Reserved.
#

######################################################################
# Config stuff for Neutrino
######################################################################

include $(MOD_DEPTH)/config/UNIX.mk

#
# XXX
# Temporary define for the Client; to be removed when binary release is used
#
ifdef MOZILLA_CLIENT
ifneq ($(USE_PTHREADS),1)
CLASSIC_NSPR = 1
endif
endif

#
# The default implementation strategy for Linux is pthreads.
#
ifeq ($(CLASSIC_NSPR),1)
IMPL_STRATEGY		= _EMU
DEFINES			+= -D_PR_LOCAL_THREADS_ONLY
else
USE_PTHREADS		= 1
IMPL_STRATEGY		= _PTH
DEFINES			+= -D_REENTRANT
endif


AR			 = qcc -Vgcc_ntox86 -M -a $@
CC			 = qcc -Vgcc_ntox86
LD			 = $(CC)
CCC			 = $(CC)

# Old Flags  -DNO_REGEX   -DSTRINGS_ALIGNED

OS_CFLAGS	 = -Wc,-Wall -Wc,-Wno-parentheses -DNTO  \
			   -D_QNX_SOURCE -DHAVE_POINTER_LOCALTIME_R -shared

COMPILER_TAG = _qcc
MKSHLIB		 = qcc -Vgcc_ntox86 -shared -Wl,-h$(@:$(OBJDIR)/%.so=%.so) -M

RANLIB		 = ranlib
G++INCLUDES	 =
OS_LIBS		 =
EXTRA_LIBS  = -lsocket

ifdef BUILD_OPT
OPTIMIZER = -O1
else
OPTIMIZER = -O1 -gstabs
endif

NOSUCHFILE	= /no-such-file

GARBAGE		+= *.map

