#! gmake
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

CORE_DEPTH = ../../..

include manifest.mn
include $(CORE_DEPTH)/coreconf/config.mk

# 	$(NULL)


INCLUDES += \
	-I$(DIST)/../public/security \
	-I$(DIST)/../private/security \
	-I$(DEPTH)/security/lib/cert \
	-I$(DEPTH)/security/lib/key \
	-I$(DEPTH)/security/lib/util  \
	-I./include \
	$(NULL)


# For the time being, sec stuff is export only
# US_FLAGS = -DEXPORT_VERSION -DUS_VERSION

US_FLAGS = -DEXPORT_VERSION
EXPORT_FLAGS = -DEXPORT_VERSION

BASE_LIBS = \
	$(DIST)/lib/libdbm.$(LIB_SUFFIX) \
	$(DIST)/lib/libxp.$(LIB_SUFFIX) \
	$(DIST)/lib/libnspr.$(LIB_SUFFIX) \
	$(NULL)

#	$(DIST)/lib/libpurenspr.$(LIB_SUFFIX) \

#There are a circular dependancies in security/lib, and we deal with it by 
# double linking some libraries
SEC_LIBS = \
	$(DIST)/lib/libsecnav.$(LIB_SUFFIX) \
        $(DIST)/lib/libssl.$(LIB_SUFFIX) \
        $(DIST)/lib/libpkcs7.$(LIB_SUFFIX) \
        $(DIST)/lib/libcert.$(LIB_SUFFIX) \
        $(DIST)/lib/libkey.$(LIB_SUFFIX) \
	$(DIST)/lib/libsecmod.$(LIB_SUFFIX) \
        $(DIST)/lib/libcrypto.$(LIB_SUFFIX) \
        $(DIST)/lib/libsecutil.$(LIB_SUFFIX) \
        $(DIST)/lib/libssl.$(LIB_SUFFIX) \
        $(DIST)/lib/libpkcs7.$(LIB_SUFFIX) \
        $(DIST)/lib/libcert.$(LIB_SUFFIX) \
        $(DIST)/lib/libkey.$(LIB_SUFFIX) \
	$(DIST)/lib/libsecmod.$(LIB_SUFFIX) \
        $(DIST)/lib/libcrypto.$(LIB_SUFFIX) \
        $(DIST)/lib/libsecutil.$(LIB_SUFFIX) \
        $(DIST)/lib/libhash.$(LIB_SUFFIX) \
	$(NULL)

MYLIB	= lib/$(OBJDIR)/libsectool.$(LIB_SUFFIX)

US_LIBS	= $(MYLIB) $(SEC_LIBS) $(BASE_LIBS) $(MYLIB) $(BASE_LIBS)
EX_LIBS	= $(MYLIB) $(SEC_LIBS) $(BASE_LIBS) $(MYLIB) $(BASE_LIBS) 

REQUIRES = libxp nspr security

CSRCS	= $(EXEC_SRCS) $(BI_SRCS)

OBJS	= $(CSRCS:.c=.o) $(BI_SRCS:.c=-us.o) $(BI_SRCS:.c=-ex.o)

PROGS		= $(addprefix $(OBJDIR)/, $(EXEC_SRCS:.c=$(BIN_SUFFIX)))
US_PROGS 	= $(addprefix $(OBJDIR)/, $(BI_SRCS:.c=-us$(BIN_SUFFIX)))
EX_PROGS	= $(addprefix $(OBJDIR)/, $(BI_SRCS:.c=-ex$(BIN_SUFFIX)))


NON_DIRS = $(PROGS) $(US_PROGS) $(EX_PROGS)
TARGETS = $(NON_DIRS)

include $(CORE_DEPTH)/coreconf/rules.mk

symbols::
	@echo "TARGETS	= $(TARGETS)"
