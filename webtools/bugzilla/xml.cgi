#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
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
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Dawn Endico    <endico@mozilla.org>
#                 Terry Weissman <terry@mozilla.org>

use diagnostics;
use strict;
use Bug;
require "CGI.pl";

if (!defined $::FORM{'id'} || $::FORM{'id'} !~ /^\s*\d+(,\d+)*\s*$/) {
  print "Content-type: text/html\n\n";
  PutHeader("Display as XML");
  print "<FORM METHOD=GET ACTION=\"xml.cgi\">\n";
  print "Display bugs as XML by entering a list of bug numbers here:\n";
  print "<INPUT NAME=id>\n";
  print "<INPUT TYPE=\"submit\" VALUE=\"Display as XML\"><br>\n";
  print "  (e.g. 1000,1001,1002)\n";
  print "</FORM>\n";
  PutFooter();
  exit;
}

quietly_check_login();
my $exporter;
if (defined $::COOKIE{"Bugzilla_login"}) {
  $exporter = $::COOKIE{"Bugzilla_login"};
}

my @ids = split ( /,/, $::FORM{'id'} );

print "Content-type: text/plain\n\n";
print Bug::XML_Header( Param("urlbase"), $::param{'version'}, 
                        Param("maintainer"), $exporter );
foreach my $id (@ids) {
  my $bug = new Bug($id, $::userid);
  print $bug->emitXML;
}

print Bug::XML_Footer;
