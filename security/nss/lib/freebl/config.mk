#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Netscape security libraries.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1994-2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# only do this in the outermost freebl build.
ifndef FREEBL_RECURSIVE_BUILD
# we only do this stuff for some of the 32-bit builds, no 64-bit builds
ifndef USE_64

ifeq ($(OS_TARGET), HP-UX)
  ifneq ($(OS_TEST), ia64)
    FREEBL_EXTENDED_BUILD = 1
  endif
endif

ifeq ($(OS_TARGET),SunOS)
  ifeq ($(CPU_ARCH),sparc)
    FREEBL_EXTENDED_BUILD = 1
  endif
endif

ifdef FREEBL_EXTENDED_BUILD
# We're going to change this build so that it builds libfreebl.a with
# just loader.c.  Then we have to build this directory twice again to 
# build the two DSOs.
# To build libfreebl.a with just loader.c, we must now override many
# of the make variables setup by the prior inclusion of CORECONF's config.mk

CSRCS		= loader.c sysrand.c
SIMPLE_OBJS 	= $(CSRCS:.c=$(OBJ_SUFFIX))
OBJS 		= $(addprefix $(OBJDIR)/$(PROG_PREFIX), $(SIMPLE_OBJS))
ALL_TRASH :=    $(TARGETS) $(OBJS) $(OBJDIR) LOGS TAGS $(GARBAGE) \
                $(NOSUCHFILE) so_locations 
endif

#end of 32-bit only stuff.
endif

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
# This is a recursive build.  

TARGETS	     = $(SHARED_LIBRARY)
LIBRARY      =
PROGRAM      =

#ifeq ($(OS_TARGET), HP-UX)
  EXTRA_LIBS        += \
	$(DIST)/lib/libsecutil.$(LIB_SUFFIX) \
	$(NULL)

# $(PROGRAM) has NO explicit dependencies on $(EXTRA_SHARED_LIBS)
# $(EXTRA_SHARED_LIBS) come before $(OS_LIBS), except on AIX.
  EXTRA_SHARED_LIBS += \
	-L$(DIST)/lib/ \
	-lplc4 \
	-lplds4 \
	-lnspr4 \
	-lc
#endif

endif
