#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.
# 

DEPTH		= ../../..

MODULE		= pref
LIBRARY_NAME	= pref

REQUIRES	= libxp js pref nspr dbm security ldap java img layer util

ifneq ($(subst /,_,$(shell uname -s)),OS2)
CSRCS		= unix/unixpref.c
else
CSRCS		= prefldap.c
LIBRARY		= xppref32.$(LIB_SUFFIX)
EXPORTS		= initpref.js init/all.js init/config.js win/winpref.js os2/os2pref.js
OS2_IMPLIB	= 1
DEF_FILE	= xpOS2.def
EXTRA_LIBS	= $(DIST)/lib/js$(MOZ_BITS)$(VERSION_NUMBER).lib \
		  $(LIBNSPR) \
		  $(DIST)/lib/libldap.lib $(DIST)/lib/liblber.lib \
		  $(DIST)/lib/libldap.lib $(DIST)/lib/libhash.lib \
		  $(NULL)
endif

CSRCS		+= prefapi.c


GARBAGE		+= unix/jsbuffer.h

CONFIG_FILES	= initpref.js \
		  init/all.js \
		  $(NULL)

ifdef MOZ_MAIL_NEWS
CONFIG_FILES	+= \
		  init/mailnews.js \
		  $(NULL)
endif

ifdef MOZ_SECURITY
CONFIG_FILES	+= \
		  init/security.js \
		  $(NULL)
endif

ifdef MOZ_EDITOR
CONFIG_FILES	+= \
		  init/editor.js \
		  $(NULL)
endif

ifndef MOZ_MAIL_NEWS
CONFIG_FILES	+= \
		  init/configr.js \
		  $(NULL)
else
CONFIG_FILES	+= \
		  init/config.js \
		  $(NULL)
endif

CONFIG_FILES	+= \
		  unix/unix.js \
		  $(NULL)

HEADER_DEST_DIR = prefnames
# figure out what the headers are (including paths)
CONFIG_FILE_HEADERS = $(notdir $(CONFIG_FILES:%.js=%.h))

# strip out the path and add the dest and pref_ prefix
CONFIG_FILE_EXPORTS = $(addprefix $(HEADER_DEST_DIR)/pref_,$(CONFIG_FILE_HEADERS))

EXPORTS += $(CONFIG_FILE_EXPORTS)

include $(DEPTH)/config/rules.mk

INCLUDES	+= -I$(DEPTH)/cmd/xfe/icons -Iunix

$(LIBRARY): $(OBJS)


unix/unixpref.c: unix/jsbuffer.h


unix/jsbuffer.h: Makefile $(CONFIG_FILES)
	@echo "/* This is an automatically generated file */" > $@
	@echo >> $@
	@echo "static char* pref_init_buffer = " >> $@
	@cat $(CONFIG_FILES) | sed 's/\\/\\\\/g' | sed 's/\\r/\\n/' | sed 's/\"/\\\"/g' | sed 's/^M//g' | sed 's/^/"/' | sed 's/$$/\\n"/' >> $@
	@echo \; >> $@

$(HEADER_DEST_DIR):
	mkdir $(HEADER_DEST_DIR)

# these rules are ugly because we have already stripped out
# path of the source .js file
# (i.e. we've lost the file.h -> sourcedir/source.h mapping)

# we have to make seperate rules for each subdirectory to appease gmake
# (if anyone knows how to do this more cleanly, feel free)

$(HEADER_DEST_DIR)/pref_%.h: init/%.js $(HEADER_DEST_DIR)
	$(PERL) prefheader.pl $< > $@

$(HEADER_DEST_DIR)/pref_%.h: unix/%.js $(HEADER_DEST_DIR)
	$(PERL) prefheader.pl $< > $@

$(HEADER_DEST_DIR)/pref_%.h: %.js 	$(HEADER_DEST_DIR)
	$(PERL) prefheader.pl $< > $@
