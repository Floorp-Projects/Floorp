#!/usr/bonsaitools/bin/perl -wT
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
#                 Gervase Markham <gerv@gerv.net>

use diagnostics;
use strict;

use lib qw(.);

use Bug;
require "CGI.pl";

use vars qw($template $vars);

if (!defined $::FORM{'id'} || !$::FORM{'id'}) {
    print "Content-Type: text/html\n\n";
    $template->process("bug/choose-xml.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

quietly_check_login();

my $exporter = $::COOKIE{"Bugzilla_login"} || undef;

my @ids = split (/[, ]+/, $::FORM{'id'});

print "Content-type: text/xml\n\n";
print Bug::XML_Header(Param("urlbase"), $::param{'version'}, 
                      Param("maintainer"), $exporter);
foreach my $id (@ids) {
  my $bug = new Bug(trim($id), $::userid);
  print $bug->emitXML;
}

print Bug::XML_Footer;
