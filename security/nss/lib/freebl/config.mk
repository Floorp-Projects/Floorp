# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# only do this in the outermost freebl build.
ifndef FREEBL_CHILD_BUILD

# We're going to change this build so that it builds libfreebl.a with
# just loader.c.  Then we have to build this directory twice again to 
# build the two DSOs.
# To build libfreebl.a with just loader.c, we must now override many
# of the make variables setup by the prior inclusion of CORECONF's config.mk

CSRCS		= loader.c 
SIMPLE_OBJS 	= $(CSRCS:.c=$(OBJ_SUFFIX))
OBJS 		= $(addprefix $(OBJDIR)/$(PROG_PREFIX), $(SIMPLE_OBJS))
ALL_TRASH :=    $(TARGETS) $(OBJS) $(OBJDIR) LOGS TAGS $(GARBAGE) \
                $(NOSUCHFILE) so_locations 

# this is not a recursive child make.  We make a static lib. (archive)

# Override the values defined in coreconf's ruleset.mk.
#
# - (1) LIBRARY: a static (archival) library
# - (2) SHARED_LIBRARY: a shared (dynamic link) library
# - (3) IMPORT_LIBRARY: an import library, used only on Windows
# - (4) PROGRAM: an executable binary
#
# override these variables to prevent building a DSO/DLL.
  TARGETS        = $(LIBRARY)
  SHARED_LIBRARY =
  IMPORT_LIBRARY =
  PROGRAM        =

else

# This is a recursive child make. We build the shared lib.

TARGETS      = $(SHARED_LIBRARY)
LIBRARY      =
IMPORT_LIBRARY =
PROGRAM      =

ifeq ($(OS_TARGET), SunOS)
OS_LIBS += -lkstat
endif

ifeq (,$(filter-out WIN%,$(OS_TARGET)))

# don't want the 32 in the shared library name
SHARED_LIBRARY = $(OBJDIR)/$(DLL_PREFIX)$(LIBRARY_NAME)$(LIBRARY_VERSION).$(DLL_SUFFIX)

RES     = $(OBJDIR)/$(LIBRARY_NAME).res
RESNAME = freebl.rc

ifndef WINCE
ifdef NS_USE_GCC
OS_LIBS += -lshell32
else
OS_LIBS += shell32.lib
endif
endif

ifdef NS_USE_GCC
EXTRA_SHARED_LIBS += \
	-L$(DIST)/lib \
	-L$(NSSUTIL_LIB_DIR) \
	-lnssutil3 \
	-L$(NSPR_LIB_DIR) \
	-lnspr4 \
	$(NULL)
else # ! NS_USE_GCC
EXTRA_SHARED_LIBS += \
	$(DIST)/lib/nssutil3.lib \
	$(NSPR_LIB_DIR)/$(NSPR31_LIB_PREFIX)nspr4.lib \
	$(NULL)
endif # NS_USE_GCC

else

ifeq ($(FREEBL_NO_DEPEND),1)
#drop pthreads as well
OS_PTHREAD=
else
EXTRA_SHARED_LIBS += \
	-L$(DIST)/lib \
	-L$(NSSUTIL_LIB_DIR) \
	-lnssutil3 \
	-L$(NSPR_LIB_DIR) \
	-lnspr4 \
	$(NULL)
endif
endif

ifeq ($(OS_ARCH), Darwin)
EXTRA_SHARED_LIBS += -dylib_file @executable_path/libplc4.dylib:$(DIST)/lib/libplc4.dylib -dylib_file @executable_path/libplds4.dylib:$(DIST)/lib/libplds4.dylib
endif

endif
