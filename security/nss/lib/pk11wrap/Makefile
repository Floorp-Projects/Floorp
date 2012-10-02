#! gmake
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#######################################################################
# (1) Include initial platform-independent assignments (MANDATORY).   #
#######################################################################

include manifest.mn

#######################################################################
# (2) Include "global" configuration information. (OPTIONAL)          #
#######################################################################

include $(CORE_DEPTH)/coreconf/config.mk

#######################################################################
# (3) Include "component" configuration information. (OPTIONAL)       #
#######################################################################



#######################################################################
# (4) Include "local" platform-dependent assignments (OPTIONAL).      #
#######################################################################

-include config.mk

#######################################################################
# (5) Execute "global" rules. (OPTIONAL)                              #
#######################################################################

include $(CORE_DEPTH)/coreconf/rules.mk

#######################################################################
# (6) Execute "component" rules. (OPTIONAL)                           #
#######################################################################



#######################################################################
# (7) Execute "local" rules. (OPTIONAL).                              #
#######################################################################

export:: private_export

$(OBJDIR)/pk11load$(OBJ_SUFFIX): debug_module.c

# On AIX 4.3, IBM xlC_r compiler (version 3.6.6) cannot compile
# pk11slot.c in 64-bit mode for unknown reasons.  A workaround is
# to compile it with optimizations turned on.  (Bugzilla bug #63815)
ifeq ($(OS_TARGET)$(OS_RELEASE),AIX4.3)
ifeq ($(USE_64),1)
ifndef BUILD_OPT
$(OBJDIR)/pk11slot.o: pk11slot.c
	@$(MAKE_OBJDIR)
	$(CC) -o $@ -c -O2 $(CFLAGS) $<
endif
endif
endif
