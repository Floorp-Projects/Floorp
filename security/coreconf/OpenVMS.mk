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

#
# Config stuff for Compaq OpenVMS
#

include $(CORE_DEPTH)/coreconf/UNIX.mk

ifdef INTERNAL_TOOLS
CC			= c89
CCC			= cxx
OPTIMIZER		= -O
else
CC			= ccc
CCC			= ccc
endif

RANLIB			= /bin/true

CPU_ARCH		:= $(shell uname -Wh)

OS_CFLAGS              = -DVMS -DVMS_AS_IS -Wc,names=\(short,as\) \
                         -DGENERIC_PTHREAD_REDEFINES -DNO_UDSOCK
OS_CXXFLAGS            = -DVMS -DVMS_AS_IS -Wc,names=\(short,as\) \
                         -DGENERIC_PTHREAD_REDEFINES -DNO_UDSOCK

# Maybe this should go into rules.mk or something?
ifdef NSPR_INCLUDE_DIR
INCLUDES += -I$(NSPR_INCLUDE_DIR)
endif

#
# XCFLAGS are the only CFLAGS that are used during a link operation. Defining
# OPTIMIZER in XCFLAGS means that each compilation line gets OPTIMIZER
# included twice, but at least we get OPTIMIZER included in the link
# operations; and OpenVMS needs it!
#
XCFLAGS                        += $(OPTIMIZER)

# The command to build a shared library in POSIX on OpenVMS.
MKSHLIB = vmsld_psm OBJDIR=$(OBJDIR) $(OPTIMIZER)
ifdef MAPFILE
# Add LD options to restrict exported symbols to those in the map file
endif
# Change PROCESS to put the mapfile in the correct format for this platform
PROCESS_MAP_FILE = copy $(LIBRARY_NAME).def $@

