#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
# 
# The Original Code is the Bugzilla Bug Tracking System.
# 
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.
# 
# Contributor(s): Terry Weissman <terry@mozilla.org>

use diagnostics;
use strict;

use vars @::FORM;

require "CGI.pl";

confirm_login();

print "Content-type: text/html\n\n";

if (!defined $::FORM{'id'} || $::FORM{'id'} eq "") {
    PutHeader("Search By Bug Number", "Search By Bug Number", "");
    print "<FORM METHOD=\"GET\" ACTION=\"show_bug.cgi\">\n";
    print "You may find a single bug by entering its bug id here: \n";
    print "<INPUT NAME=\"id\">\n";
    print "<INPUT TYPE=\"submit\" VALUE=\"Show Me This Bug\">\n";
    print "</FORM>\n";
    exit;
}

ConnectToDatabase();

GetVersionTable();

PutHeader("Bugzilla bug $::FORM{'id'}", "Bugzilla Bug", $::FORM{'id'});
navigation_header();

print "<HR>\n";

do "bug_form.pl";
