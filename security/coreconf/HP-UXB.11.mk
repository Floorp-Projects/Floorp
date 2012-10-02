#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
include $(CORE_DEPTH)/coreconf/HP-UX.mk

ifndef NS_USE_GCC
    CCC                 = /opt/aCC/bin/aCC -ext
    ifeq ($(USE_64), 1)
	ifeq ($(OS_TEST), ia64)
	    ARCHFLAG	= -Aa +e +p +DD64
	else
	    # Our HP-UX build machine has a strange problem.  If
	    # a 64-bit PA-RISC executable calls getcwd() in a
	    # network-mounted directory, it fails with ENOENT.
	    # We don't know why.  Since nsinstall calls getcwd(),
	    # this breaks our 64-bit HP-UX nightly builds.  None
	    # of our other HP-UX machines have this problem.
	    #
	    # We worked around this problem by building nsinstall
	    # as a 32-bit PA-RISC executable for 64-bit PA-RISC
	    # builds.  -- wtc 2003-06-03
	    ifdef INTERNAL_TOOLS
	    ARCHFLAG	= +DAportable +DS2.0
	    else
	    ARCHFLAG	= -Aa +e +DA2.0W +DS2.0 +DChpux
	    endif
	endif
    else
	ifeq ($(OS_TEST), ia64)
	    ARCHFLAG	= -Aa +e +p +DD32
	else
	    ARCHFLAG	= +DAportable +DS2.0
	endif
    endif
else
    CCC = aCC
endif

#
# To use the true pthread (kernel thread) library on HP-UX
# 11.x, we should define _POSIX_C_SOURCE to be 199506L.
# The _REENTRANT macro is deprecated.
#

OS_CFLAGS += $(ARCHFLAG) -DHPUX11 -D_POSIX_C_SOURCE=199506L
OS_LIBS   += -lpthread -lm -lrt
HPUX11	= 1
