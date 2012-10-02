# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
MAKEFILE_CVS_ID = "@(#) $RCSfile: Makefile,v $ $Revision: 1.7 $ $Date: 2012/04/25 14:49:42 $"

include manifest.mn
include $(CORE_DEPTH)/coreconf/config.mk
include config.mk
include $(CORE_DEPTH)/coreconf/rules.mk

# On AIX 4.3, IBM xlC_r compiler (version 3.6.6) cannot compile
# ckhelper.c in 64-bit mode for unknown reasons.  A workaround is
# to compile it with optimizations turned on.  (Bugzilla bug #63815)
ifeq ($(OS_TARGET)$(OS_RELEASE),AIX4.3)
ifeq ($(USE_64),1)
ifndef BUILD_OPT
$(OBJDIR)/ckhelper.o: ckhelper.c
	@$(MAKE_OBJDIR)
	$(CC) -o $@ -c -O2 $(CFLAGS) $<
endif
endif
endif

export:: private_export
