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

######################################################################
# Config stuff for Sony NEWS-OS
######################################################################
 
######################################################################
# Version-independent
######################################################################

include $(MOD_DEPTH)/config/UNIX.mk

ARCH			:= sony
CPU_ARCH		:= mips
 
CC			= cc
CCC			= CC
RANLIB			= /bin/true

OS_INCLUDES		= -I/usr/include
G++INCLUDES		=
#OS_LIBS			= -lsocket -lnsl -lgen -lresolv

PLATFORM_FLAGS		= -Xa -fullwarn -DSONY
PORT_FLAGS		= -DSYSV -DSVR4 -D__svr4 -D__svr4__ -D_PR_LOCAL_THREADS_ONLY -DHAVE_SVID_GETTOD

OS_CFLAGS		= $(PLATFORM_FLAGS) $(PORT_FLAGS)

######################################################################
# Version-specific stuff
######################################################################

######################################################################
# Overrides for defaults in config.mk (or wherever)
######################################################################

######################################################################
# Other
######################################################################

MKSHLIB			= $(LD) $(DSO_LDOPTS)
 
DSO_LDOPTS		= -G
DSO_LDFLAGS		=
