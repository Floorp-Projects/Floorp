#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(CORE_DEPTH)/coreconf/UNIX.mk

LIB_SUFFIX  = a
DLL_SUFFIX  = so
AR          = ar cr $@
LDOPTS     += -L$(SOURCE_LIB_DIR)
MKSHLIB     = $(CC) $(DSO_LDOPTS) -Wl,-soname -Wl,$(@:$(OBJDIR)/%.so=%.so)

OS_RELEASE  =
OS_TARGET   = RISCOS

DSO_CFLAGS  = -fPIC
DSO_LDOPTS  = -shared

ifdef BUILD_OPT
	OPTIMIZER = -O3
endif
