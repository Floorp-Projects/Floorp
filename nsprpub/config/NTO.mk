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

CPU_ARCH	 = x86
AR			 = qcc -Vgcc_ntox86 -M -a $@
CC			 = qcc -Vgcc_ntox86 -shared 
LD			 = $(CC)
CCC			 = $(CC)
OS_CFLAGS	 = -Wc,-Wall -Wc,-Wno-parentheses -DNTO -DNTO2 -Di386 \
			   -D_QNX_SOURCE -DNO_REGEX \
			   -DSTRINGS_ALIGNED -D__i386__ -D__QNXNTO__ -DPIC \
			   -DHAVE_POINTER_LOCALTIME_R
COMPILER_TAG = _qcc
MKSHLIB		 = qcc -Vgcc_ntox86 -shared -Wl,-h$(@:$(OBJDIR)/%.so=%.so) -g2 -M

RANLIB		 = ranlib
G++INCLUDES	 =
OS_LIBS		 =
XLDOPTS		 =

ifdef USE_PTHREADS
	IMPL_STRATEGY	= _PTH
else
	IMPL_STRATEGY   = _EMU
endif

NOSUCHFILE	= /no-such-file

GARBAGE		= $(wildcard *.err)

