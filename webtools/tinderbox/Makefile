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


# This Makefile helps you install Tinderbox.  Define PERL and MYSQLTCL to
# the full pathnames of where you have these utilities.  Define PREFIX
# to where you will install the running Tinderbox.  Then "make install" should
# copy things for you.

#BONSAI = /home/httpd/html/bonsai
#CVSROOT = /cvsroot
#GZIP = /usr/bin/gzip
#PERL = /usr/bin/perl
#PREFIX = /home/httpd/html/tinderbox
#UUDECODE = /usr/bin/uudecode

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
imagelog.pl		\
processbuild.pl	\
showbuilds.cgi	\
showimages.cgi	\
showlog.cgi		\
tbglobals.pl	\
Empty.html		\
faq.html		\
index.html		\
examples/build-moz-smoke.pl \
examples/buildit.config 	\
examples/mozilla-unix.pl 	\
examples/mozilla-windows.pl

PICS = \
1afi003r.gif	\
star.gif

install:
	mkdir -p $(PREFIX) && \
	mkdir -p $(PREFIX)/examples &&
	for I in $(FILES); do \
	  echo Installing $$I && \
          sed -e s#/usr/bonsaitools/bin/perl#$(PERL)#g \
              -e s#/tools/ns/bin/perl5#$(PERL)#g \
              -e s#/m/src#$(CVSROOT)#g \
              -e s#/usr/local/bin/gzip#$(GZIP)#g \
              -e s#/tools/ns/bin/uudecode#$(UUDECODE)#g \
              -e s#/d/webdocs/projects/bonsai#$(BONSAI)#g \
              $$I > $(PREFIX)/$$I && \
	chmod 755 $(PREFIX)/$$I; done && \
	for I in $(PICS); do \
	  echo Installing $$I && \
	  cp -f $$I $(PREFIX) && \
	  chmod 755 $(PREFIX)/$$I; done

