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

MOD_DEPTH = ..

export NSPR20=1

include $(MOD_DEPTH)/config/config.mk

DIRS = ds libc

ifneq (,$(filter-out Rhapsody WIN16 NEWS-OS,$(OS_TARGET)))
DIRS += prstreams
endif

include $(MOD_DEPTH)/config/rules.mk

export:: $(TARGETS)

install:: export
