#! gmake
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

include manifest.mn
include $(CORE_DEPTH)/coreconf/config.mk
include config.mk

CILIB                   = $(OBJDIR)/cilib.$(LIB_SUFFIX)
ORIG_CILIB              = libci/$(OS_TARGET)$(OS_RELEASE).$(LIB_SUFFIX)

ifeq (,$(filter-out WINNT WINCE,$(OS_TARGET)))  # omits WIN16 WIN95
ORIG_CILIB              = libci/tssp32.lib
endif

ifeq ($(OS_TARGET), WIN16)
ORIG_CILIB              = libci/tssp.lib
endif

ifeq ($(OS_TARGET), WIN95)
ORIG_CILIB              = libci/tssp32.lib
endif

ifeq (,$(filter-out WIN%,$(OS_TARGET)))
STUBDLL                 = $(OBJDIR)/stub.$(DLL_SUFFIX)
endif

STUBLIB                 = $(OBJDIR)/stub.$(LIB_SUFFIX)

ifeq ($(OS_TARGET), WIN16)
W16LIBS         += $(CILIB)
else
EXTRA_LIBS      += $(CILIB)
endif

INST_JS                 = inst.js
LIBCI_JAR               = $(OBJDIR)/libfort.jar
LIBCI_JAR_SRC           = $(INST_JS) $(SHARED_LIBRARY)

ifneq ($(OS_TARGET), WIN16)
TARGETS : $(LIBCI_JAR) 
endif

ifeq ($(OS_TARGET), WIN16)
# note that rules.mk is not included below for WIN16
all:
	@echo Skipping fortcrypt directory for 16-bit windows builds

all_platforms alltags clean clobber clobber_all realclean: all

boot export install libs program release: all

endif

$(SHARED_LIBRARY): $(CILIB) $(DIRS)

cilib_name:
	@echo $(CILIB)

$(CILIB):
	@$(MAKE_OBJDIR)
	@if test -f $(ORIG_CILIB); then \
		echo "Copying $(ORIG_CILIB) to $@"; \
		cp $(ORIG_CILIB) $@; \
	else \
		echo "Making empty stub $@"; \
		$(MAKE) $(STUBLIB); \
	fi
	@$(RANLIB) $@

$(STUBLIB): $(OBJDIR)/maci$(OBJ_SUFFIX)
	@$(MAKE_OBJDIR)
ifeq (,$(filter-out WIN%,$(OS_TARGET)))
	$(MAKE) $(STUBDLL)
else
	$(AR) $<
endif
	cp $@ $(CILIB)

ifeq (,$(filter-out WIN%,$(OS_TARGET)))
$(STUBDLL): $(OBJDIR)/maci.o
ifdef NS_USE_GCC
	$(LINK_DLL) -Wl,--out-implib,$(STUBLIB) $(OBJDIR)/maci.o $(OS_LIBS)
else
	$(LINK_DLL) -MAP $(DLLBASE) $(subst /,\\,$(OBJDIR)/maci.o $(OS_LIBS))
endif

$(OBJDIR)/maci.o: maci.c
ifdef NS_USE_GCC
	$(CC) -o $@ -c $(CFLAGS) $<
else
	$(CC) -Fo$@ -c $(CFLAGS) $<
endif
endif

#
# The following rules packages the shared library into a JAR,
# ready to be signed
#
$(OBJDIR)/replace: replace.c
	$(CC) -o $@ $<

# ZIP options:
# -5 means medium compression
# -q means quiet
# -j means do not store tree structure, all files go into one dir
#
$(LIBCI_JAR): $(DIRS) $(LIBCI_JAR_SRC)
	@echo +++ building $@ from $(LIBCI_JAR_SRC)
	@rm -f $@
	zip -5qj $@ $(LIBCI_JAR_SRC) 

force:
	(cd swfort ; $(MAKE))


MD_FILES += $(LIBCI_JAR)

# coreconf doesn't build the AIX shared library for FORTEZZA,
# so I'm going to override their shared library command and build the shared
# library the way config used to.
#
ifeq ($(OS_TARGET)$(OS_RELEASE), AIX4.1)
DSO_LDOPTS              = -bM:SRE -bh:4 -bnoentry
EXTRA_DSO_LDOPTS        = -lc
MKSHLIB                 = svld $(DSO_LDOPTS)

$(SHARED_LIBRARY): $(OBJS)
	@$(MAKE_OBJDIR)
	rm -f $@
	$(MKSHLIB) -o $@ $(OBJS) $(EXTRA_LIBS) $(EXTRA_DSO_LDOPTS)
	chmod +x $@
endif

ifeq ($(OS_TARGET)$(OS_RELEASE), AIX4.2)
LD      += -G
endif 


ifneq ($(OS_TARGET), WIN16)
include $(CORE_DEPTH)/coreconf/rules.mk
endif

export:: private_export


