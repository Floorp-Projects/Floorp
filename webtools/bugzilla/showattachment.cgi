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
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Jacob Steenhagen <jake@acutex.net>

use diagnostics;
use strict;

require "CGI.pl";

ConnectToDatabase();

quietly_check_login();

if ($::FORM{attach_id} !~ /^[1-9][0-9]*$/) {
    DisplayError("Attachment ID should be numeric.");
    exit;
}

SendSQL("select bug_id, mimetype, thedata from attachments where attach_id = $::FORM{'attach_id'}");
my ($bug_id, $mimetype, $thedata) = FetchSQLData();

if (!$bug_id) {
    DisplayError("Attachment $::FORM{attach_id} does not exist.");
    exit;
}

# Make sure the user can see the bug to which this file is attached
ValidateBugID($bug_id);

print qq{Content-type: $mimetype\n\n$thedata};

    
