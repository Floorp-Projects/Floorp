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

ConnectToDatabase();

use vars qw($template $vars $userid);

use Bug;

if ($::FORM{'GoAheadAndLogIn'}) {
    confirm_login();
} else {
    quietly_check_login();
}

######################################################################
# Begin Data/Security Validation
######################################################################

unless (defined ($::FORM{'id'})) {
    my $format = GetFormat("bug/choose", $::FORM{'format'}, $::FORM{'ctype'});

    print "Content-type: $format->{'contenttype'}\n\n";
    $template->process("$format->{'template'}", $vars) ||
      ThrowTemplateError($template->error());
    exit;
}

my $format = GetFormat("bug/show", $::FORM{'format'}, $::FORM{'ctype'});

# Make sure the bug ID is a positive integer representing an existing
# bug that the user is authorized to access.
ValidateBugID($::FORM{'id'});

######################################################################
# End Data/Security Validation
######################################################################

GetVersionTable();

my $bug = new Bug($::FORM{'id'}, $userid);

$vars->{'bug'} = $bug;

ThrowCodeError("bug_error") if $bug->error;

# Next bug in list (if there is one)
my @bug_list;
if ($::COOKIE{"BUGLIST"}) {
    @bug_list = split(/:/, $::COOKIE{"BUGLIST"});
}
$vars->{'bug_list'} = \@bug_list;

print "Content-type: $format->{'ctype'}\n\n";
$template->process("$format->{'template'}", $vars)
  || ThrowTemplateError($template->error());

