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

use vars qw($cgi $template $vars $userid);

use Bug;

if ($::FORM{'GoAheadAndLogIn'}) {
    confirm_login();
} else {
    quietly_check_login();
}

# Editable, 'single' HTML bugs are treated slightly specially in a few places
my $single = !$cgi->param('format')
  && (!$cgi->param('ctype') || $cgi->param('ctype') eq 'html');

# If we don't have an ID, _AND_ we're only doing a single bug, then prompt
if (!defined $cgi->param('id') && $single) {
    print "Content-type: text/html\n\n";
    $template->process("bug/choose.html.tmpl", $vars) ||
      ThrowTemplateError($template->error());
    exit;
}

my $format = GetFormat("bug/show", $::FORM{'format'}, $::FORM{'ctype'});

GetVersionTable();

my @bugs = ();

if ($single) {
    my $id = $cgi->param('id');
    # Its a bit silly to do the validation twice - that functionality should
    # probably move into Bug.pm at some point
    ValidateBugID($id);
    push @bugs, new Bug($id, $userid);
} else {
    foreach my $id ($cgi->param('id')) {
        my $bug = new Bug($id, $userid);
        push @bugs, $bug;
    }
}

$vars->{'bugs'} = \@bugs;

# Next bug in list (if there is one)
my @bug_list;
if ($::COOKIE{"BUGLIST"}) {
    @bug_list = split(/:/, $::COOKIE{"BUGLIST"});
}
$vars->{'bug_list'} = \@bug_list;

print "Content-type: $format->{'ctype'}\n\n";
$template->process("$format->{'template'}", $vars)
  || ThrowTemplateError($template->error());

