# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This makefile appends to the variable OBJS the platform-dependent
# object modules that will be part of the nspr20 library.

include $(srcdir)/md/beos/bsrcs.mk

OBJS += $(MDCSRCS:%.c=md/beos/$(OBJDIR)/%.$(OBJ_SUFFIX))
