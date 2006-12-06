#!/usr/bin/perl -wT
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
# The Original Code is the Bugzilla Testopia System.
#
# The Initial Developer of the Original Code is Greg Hendricks.
# Portions created by Greg Hendricks are Copyright (C) 2006
# Novell. All Rights Reserved.
#
# Contributor(s): Greg Hendricks <ghendricks@novell.com>

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Util;
use Bugzilla::Testopia::Util;
use JSON;

use vars qw($vars);
my $template = Bugzilla->template;
my $cgi = Bugzilla->cgi;

print $cgi->header;

my $action = $cgi->param('action') || '';

if ($action eq 'getenv'){

    my $search = $cgi->param('search');
    trick_taint($search);
    $search = "%$search%";
    my $dbh = Bugzilla->dbh;
    
    # The order of name and environment are important here.
    # JSON will convert this to an array of arrays which Dojo will interpret
    # as a select list in the ComboBox widget.
    my $ref = $dbh->selectall_arrayref(
        "SELECT name, environment_id 
           FROM test_environments 
          WHERE name like ?",
          undef, $search);
    print STDERR objToJson($ref);
    print objToJson($ref);  
}
else{
    $template->process("testopia/quicksearch.html.tmpl", $vars) ||
        ThrowTemplateError($template->error());
}