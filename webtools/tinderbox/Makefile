#!gmake
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 


# This Makefile helps you install Tinderbox.  
# Set the variables below to their proper locations.
# TINDERBOX_DIR is the full path to where you want to install tinderbox
# BONSAI_DIR is the full path to your local bonsai installation
# BONSAI_URL is the root URL to your bonsai install (e.g. ../bonsai)
# The other variables contain full paths to the utitlies required to
# use tinderbox.

# Then "make install" should copy the files to the tinderbox location
# specified by TINDERBOX_DIR

#TINDERBOX_DIR = /var/www/html/tinderbox
#BONSAI_DIR = /var/www/html/bonsai
#BONSAI_URL = /bonsai

# Set these if you want to tighten permissions
#TINDERBOX_USER = tinderbox
#TINDERBOX_GROUP = tinderbox

#CVSROOT = /cvsroot
#GZIP = /usr/bin/gzip
#PERL = /usr/bin/perl
#UUDECODE = /usr/bin/uudecode

ALL_UTILS = GZIP PERL UUDECODE

FILES = \
Backwards.pm	\
addimage.cgi	\
addnote.cgi		\
admintree.cgi	\
buildwho.pl		\
clean.pl		\
doadmin.cgi		\
ep_mac.pl		\
ep_unix.pl		\
ep_windows.pl	\
fixupimages.pl	\
handlemail.pl	\
header.pl	\
imagelog.pl		\
processbuild.pl	\
showbuilds.cgi	\
showimages.cgi	\
showlog.cgi		\
tbglobals.pl	\
Empty.html		\
faq.html		\
index.html		\
examples/buildit.config 	\
examples/mozilla-unix.pl 	\
examples/mozilla-windows.pl

PICS = \
1afi003r.gif	\
star.gif

# Sanity check
ifeq (,$(strip $(TINDERBOX_DIR)))
$(error TINDERBOX_DIR must be set to the tinderbox install location)
endif

ifeq (,$(strip $(BONSAI_DIR)))
$(error BONSAI_DIR must be set for bonsai support to work)
endif
ifeq (,$(strip $(BONSAI_URL)))
$(error BONSAI_URL must be set for bonsai support to work)
endif

install:
	$(foreach u,$(ALL_UTILS),$(if $($u),,$(error $u must be set to the full path of the utility)))
	mkdir -p $(TINDERBOX_DIR)/examples
	set -e ; \
	for I in $(FILES); do \
	  echo Installing $$I && \
          sed -e s#/usr/bonsaitools/bin/perl#$(PERL)#g \
              -e s#/tools/ns/bin/perl5#$(PERL)#g \
              -e s#/m/src#$(CVSROOT)#g \
              -e s#/usr/local/bin/gzip#$(GZIP)#g \
              -e s#/tools/ns/bin/uudecode#$(UUDECODE)#g \
              -e s#/d/webdocs/projects/bonsai#$(BONSAI_DIR)#g \
              -e s#\.\./bonsai#$(BONSAI_URL)#g \
              $$I > $(TINDERBOX_DIR)/$$I && \
	  chmod 755 $(TINDERBOX_DIR)/$$I; \
	done
	set -e ; \
	for I in $(PICS); do \
	  echo Installing $$I && \
	  cp $$I $(TINDERBOX_DIR) && \
	  chmod 755 $(TINDERBOX_DIR)/$$I; \
	done
	mkdir -p $(TINDERBOX_DIR)/data
ifneq (,$(TINDERBOX_USER)$(TINDERBOX_GROUP))
	chown -R $(TINDERBOX_USER) $(TINDERBOX_DIR)
	chgrp -R $(TINDERBOX_GROUP) $(TINDERBOX_DIR)
	chmod 755 $(TINDERBOX_DIR)
	chmod 775 $(TINDERBOX_DIR)/data
else
	chmod 777 $(TINDERBOX_DIR)
	chmod 777 $(TINDERBOX_DIR)/data
endif
