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

MODULE		= edtplug
LIBRARY_NAME	= edtplug

REQUIRES	= java nspr edtplug

JRI_GEN		= netscape.plugin.composer.PluginManager \
		  netscape.plugin.composer.MozillaCallback \
		  netscape.plugin.composer.Composer \
		  $(NULL)

CSRCS		= edtplugStubs.c edt_nodl.c

EXPORTS		= $(JRI_GEN_DIR)/netscape_plugin_composer_Composer.h \
		  $(JRI_GEN_DIR)/netscape_plugin_composer_PluginManager.h \
		  $(JRI_GEN_DIR)/netscape_plugin_composer_MozillaCallback.h \
		  $(NULL)

include $(DEPTH)/config/rules.mk

#
# Generate edt_nodl.c (so that you can check it in)
# These make rules only works on IRIX...sigh
#
ifeq ($(OS_ARCH),IRIX)
edt_nodl.c: $(OBJDIR)/edtplugStubs.o Makefile $(DEPTH)/config/nodl.pl
	@rm -f $@
	@$(PERL) $(DEPTH)/config/nodl.pl "edt_nodl_tab" \
	    `nm -Bn $(OBJDIR)/edtplugStubs.o | egrep Java_.*_stub | awk '{print $$3;}'` > $@

endif
