#!/usr/bonsaitools/bin/perl --
# -*- Mode: perl; indent-tabs-mode: nil -*-
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
# The Original Code is this file as it was released upon February 19, 1999.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

# test.cgi - Test different subsets of config.cgi

# Send comments, improvements, bugs to Steve Lamm (slamm@netscape.com).

$field_separator = '<<fs>>';
$configure_in    = 'mozilla/configure.in';
$chrome_color    = '#F0A000';
$ENV{CVSROOT}    = ':pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot';
$ENV{PATH}       = "$ENV{PATH}:/usr/bin/ccs";

system "cvs co mozilla/configure.in";

print "Content-type: text/html\n\n\n";
  print qq(
	   <HTML>
	   <HEAD>
	   <TITLE>Configure Unix Mozilla build</TITLE>
	   </HEAD>
	   <body BGCOLOR="#FFFFFF" TEXT="#000000"LINK="#0000EE" VLINK="#551A8B" ALINK="#FF0000">
	  );

print "PATH=$ENV{PATH}\n";

#  open(OPTIONS, "/usr/ccs/bin/m4 webify-configure.m4 $configure_in|")
  open(OPTIONS, "m4 webify-configure.m4 $configure_in|")
    or die "Error parsing configure.in\n";

  foreach (<OPTIONS>) {
    print;
  }

  print "\n</body>\n</html>\n";
