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
# Contributor(s): Terry Weissman <terry@mozilla.org>

use strict;

use lib qw(.);

require "CGI.pl";
require "bug_form.pl";

ConnectToDatabase();

if ($::FORM{'GoAheadAndLogIn'}) {
    confirm_login();
} else {
    quietly_check_login();
}

######################################################################
# Begin Data/Security Validation
######################################################################

# Make sure the bug ID is a positive integer representing an existing
# bug that the user is authorized to access.
if (defined ($::FORM{'id'})) {
    ValidateBugID($::FORM{'id'});
}

######################################################################
# End Data/Security Validation
######################################################################

GetVersionTable();

print "Content-type: text/html\n\n";

show_bug();
