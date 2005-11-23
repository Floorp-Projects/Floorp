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
# The Original Code is the Netscape Portable Runtime (NSPR).
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-2000
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

#! gmake



MOD_DEPTH = ../../..

include $(MOD_DEPTH)/config/config.mk

INCLUDES = -I$(DIST)/include

CSRCS = winevent.c

ifeq ($(OS_ARCH), WINNT)
ifeq ($(OS_TARGET), WIN16)
  LIBPR = $(DIST)/lib/nspr$(MOD_VERSION).lib
  LIBPLC= $(DIST)/lib/plc$(MOD_VERSION).lib
  LIBPLDS= $(DIST)/lib/plds$(MOD_VERSION).lib
else
  LDOPTS = -NOLOGO -DEBUG -INCREMENTAL:NO
  ifeq ($(OS_TARGET), WIN95)
  LIBPR = $(DIST)/lib/nspr$(MOD_VERSION).$(LIB_SUFFIX)
  LIBPLC= $(DIST)/lib/plc$(MOD_VERSION).$(LIB_SUFFIX)
  LIBPLDS= $(DIST)/lib/plds$(MOD_VERSION).lib
  else
  LIBPR = $(DIST)/lib/libnspr$(MOD_VERSION).$(LIB_SUFFIX)
  LIBPLC= $(DIST)/lib/libplc$(MOD_VERSION).$(LIB_SUFFIX)
  LIBPLDS= $(DIST)/lib/libplds$(MOD_VERSION).lib
  endif
endif
endif

TARGETS = $(OBJDIR)/winevent.exe
OS_CFLAGS = $(OS_EXE_CFLAGS)
LDOPTS = -NOLOGO -DEBUG -INCREMENTAL:NO
LDFLAGS += -DEBUG
LIBPR += $(LIBPLDS)
LIBPR += kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib

include $(MOD_DEPTH)/config/rules.mk

$(OBJDIR)/winevent.exe: $(OBJS)
	link $(LDOPTS) $< $(LIBPLC) $(LIBPR) wsock32.lib -out:$@

export:: $(TARGETS)

clean::
	rm -rf $(TARGETS)
