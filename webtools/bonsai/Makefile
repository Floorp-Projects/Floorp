#!gmake
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.


# This Makefile helps you install Bonsai.  Define PERL and MYSQLTCL to
# the full pathnames of where you have these utilities.  Define PREFIX
# to where you will install the running Bonsai.  Then "make install" should
# copy things for you.


CFLAGS = -g
#LDFLAGS = -lcrypt
#PERL = /usr/bin/perl
#MYSQLTCL = /bonsai/bin/mysqltcl
#PREFIX = /home/httpd/html/bonsai

FILES = CGI.tcl			\
	SourceChecker.cgi	\
	SourceChecker.pm	\
	addcheckin.tcl		\
	admin.cgi		\
	adminfuncs.tcl		\
	adminmail.tcl		\
	branchspam.cgi		\
	branchspammer.cgi	\
	changebar.tcl		\
	closemessage		\
	contacthelp.html	\
	countcheckins.cgi	\
	createlegaldirs.tcl	\
	cvsblame.cgi		\
	cvsblame.pl		\
	cvsguess.cgi		\
	cvsindex.pl		\
	cvslog.cgi		\
	cvsmenu.pl		\
	cvsquery.cgi		\
	cvsquery.pl		\
	cvsqueryform.cgi	\
	cvsregexp.html		\
	cvsview2.cgi		\
	doadmin.cgi		\
	doeditcheckin.cgi	\
	doeditmessage.cgi	\
	doeditprofile.cgi	\
	doeditwhiteboard.cgi	\
	dolog.pl		\
	dotweak.cgi		\
	editcheckin.cgi		\
	editmessage.cgi		\
	editprofile.cgi		\
	editwhiteboard.cgi	\
	globals.tcl		\
	handleAdminMail.tcl	\
	handleCheckinMail.tcl	\
	header.pl		\
	index.html		\
	indextest.pl		\
	lloydcgi.pl		\
	localprofile.cgi	\
	maketables.sh		\
	moduleanalyse.cgi	\
	modules.pl		\
	multidiff.cgi		\
	myglobrecur.tcl		\
	openmessage		\
	perlifyconfig.tcl	\
	processqueue.pl		\
	profile.cgi		\
	rebuildcvshistory.cgi	\
	rebuilddatabase.tcl	\
	rebuildtaginfo.cgi	\
	repophook.cgi		\
	reposfiles.pl		\
	rview.cgi		\
	show2.cgi		\
	showcheckins.cgi	\
	switchtree.cgi		\
	testlock.pl		\
	toplevel.cgi		\
	utils.pl		\
	viewold.cgi		

trapdoor: trapdoor.o
	cc -o trapdoor trapdoor.o $(LDFLAGS)

install: trapdoor
	mkdir -p $(PREFIX) && \
	for I in $(FILES); do \
	  echo Installing $$I && \
          sed -e s#/usr/bonsaitools/bin/perl#$(PERL)#g \
              -e s#/usr/bonsaitools/bin/mysqltcl#$(MYSQLTCL)#g \
              $$I > $(PREFIX)/$$I && \
	chmod 755 $(PREFIX)$(I); done && \
	mkdir -p $(PREFIX)/data && \
	cp -f trapdoor $(PREFIX)/data && \
	chmod 755 $(PREFIX)/data && \
	cp -f bonsai.gif $(PREFIX) && \
	chmod 755 $(PREFIX)/bonsai.gif

clean:
	rm -f trapdoor trapdoor.o


