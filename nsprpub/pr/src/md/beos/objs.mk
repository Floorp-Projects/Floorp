#
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "MPL"); you may not use this file except in
# compliance with the MPL.  You may obtain a copy of the MPL at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the MPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
# for the specific language governing rights and limitations under the
# MPL.
#

# This makefile appends to the variable OBJS the platform-dependent
# object modules that will be part of the nspr20 library.

include md/beos/bsrcs.mk

OBJS += $(MDCSRCS:%.c=md/beos/$(OBJDIR)/%.$(OBJ_SUFFIX))
