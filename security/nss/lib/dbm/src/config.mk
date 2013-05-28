#! gmake
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

DEFINES += -DMEMMOVE -D__DBINTERFACE_PRIVATE

#
#  Currently, override TARGETS variable so that only static libraries
#  are specifed as dependencies within rules.mk.
#

TARGETS        = $(LIBRARY)
SHARED_LIBRARY =
IMPORT_LIBRARY =
PURE_LIBRARY   =
PROGRAM        =

ifdef SHARED_LIBRARY
	ifeq (,$(filter-out WIN%,$(OS_TARGET)))
		DLLBASE=/BASE:0x30000000
		RES=$(OBJDIR)/dbm.res
		RESNAME=../include/dbm.rc
	endif
	ifeq ($(DLL_SUFFIX),dll)
		DEFINES += -D_DLL
	endif
endif

ifeq ($(OS_TARGET),AIX)
	OS_LIBS += -lc_r
endif
