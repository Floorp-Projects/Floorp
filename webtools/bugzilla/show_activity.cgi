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
#                 Myk Melez <myk@mozilla.org>
#                 Gervase Markham <gerv@gerv.net>

use strict;

use lib qw(.);
use vars qw ($template $vars);

require "CGI.pl";

ConnectToDatabase();

###############################################################################
# Begin Data/Security Validation
###############################################################################

# Check whether or not the user is currently logged in. This function 
# sets the value of $::usergroupset, the binary number that records
# the set of groups to which the user belongs and which we can use
# to determine whether or not the user is authorized to access this bug.
quietly_check_login();

# Make sure the bug ID is a positive integer representing an existing
# bug that the user is authorized to access.
ValidateBugID($::FORM{'id'});

###############################################################################
# End Data/Security Validation
###############################################################################

($vars->{'operations'}, $vars->{'incomplete_data'}) = 
                                                 GetBugActivity($::FORM{'id'});

$vars->{'bug_id'} = $::FORM{'id'};

print "Content-type: text/html\n\n";

$template->process("bug/activity/show.html.tmpl", $vars)
  || ThrowTemplateError($template->error());

