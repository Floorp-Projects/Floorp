#! gmake
# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#

CORE_DEPTH = ../..
DEPTH = ../..

include manifest.mn
include $(CORE_DEPTH)/coreconf/config.mk

# These sources were once in this directory, but now are gone.
MISSING_SOURCES = \
	addcert.c \
	berparse.c \
	cert.c	\
	key.c	\
	key_rand.c \
	keygen.c \
	sec_fe.c \
	sec_read.c \
	secarb.c \
	secutil.c \
	$(NULL)

# we don't build these any more, but the sources are still here
OBSOLETE = \
	berdec.c \
	berdump.c \
	cypher.c \
	dumpcert.c \
	listcerts.c \
	mkdongle.c \
	p12exprt.c \
	p12imprt.c \
	rc4.c \
	sign.c \
	unwrap.c \
	vector.c \
	verify.c \
	wrap.c \
	$(NULL)

# the base files for the executables
# hey -- keep these alphabetical, please
EXEC_SRCS = \
	$(NULL)

# files that generate two separate objects and executables
# BI_SRCS	= \
# 	keyutil.c \
# 	p7env.c \
# 	tstclnt.c \
# 	$(NULL)

#	-I$(CORE_DEPTH)/security/lib/cert \
#	-I$(CORE_DEPTH)/security/lib/key \
#	-I$(CORE_DEPTH)/security/lib/util  \

INCLUDES += \
	-I$(DIST)/../public/security \
	-I./include \
	$(NULL)

TBD_DIRS = rsh rshd rdist ssld

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

#There is a circular dependancy in security/lib, and here is a gross fix
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


ifneq ($(OS_ARCH),OS2)
$(OBJDIR)/%-us.o: %.c
	@$(MAKE_OBJDIR)
	$(CCF) -o $@ $(US_FLAGS) -c $*.c

$(OBJDIR)/%-ex.o: %.c
	@$(MAKE_OBJDIR)
	$(CCF) -o $@ $(EXPORT_FLAGS) -c $*.c

$(OBJDIR)/%.o: %.c
	@$(MAKE_OBJDIR)
	$(CCF) -o $@ $(EXPORT_FLAGS) -c $*.c

$(US_PROGS):$(OBJDIR)/%-us: $(OBJDIR)/%-us.o $(US_LIBS)
	@$(MAKE_OBJDIR)
	$(CCF) -o $@ $(OBJDIR)/$*-us.o $(LDFLAGS) $(US_LIBS) $(OS_LIBS)

$(EX_PROGS):$(OBJDIR)/%-ex: $(OBJDIR)/%-ex.o $(EX_LIBS)
	@$(MAKE_OBJDIR)
	$(CCF) -o $@ $(OBJDIR)/$*-ex.o $(LDFLAGS) $(EX_LIBS) $(OS_LIBS)

$(PROGS):$(OBJDIR)/%: $(OBJDIR)/%.o $(EX_LIBS)
	@$(MAKE_OBJDIR)
	$(CCF) -o $@ $@.o $(LDFLAGS) $(EX_LIBS) $(OS_LIBS)

#install:: $(TARGETS)
#	$(INSTALL) $(TARGETS) $(DIST)/bin
endif

symbols::
	@echo "TARGETS	= $(TARGETS)"
