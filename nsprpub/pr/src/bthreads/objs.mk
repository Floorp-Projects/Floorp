# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This makefile appends to the variable OBJS the bthread object modules
# that will be part of the nspr20 library.

include $(srcdir)/bthreads/bsrcs.mk

OBJS += $(BTCSRCS:%.c=bthreads/$(OBJDIR)/%.$(OBJ_SUFFIX))
