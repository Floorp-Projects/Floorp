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
# Contributor(s): Owen Taylor <otaylor@redhat.com>
#                 Gervase Markham <gerv@gerv.net>
#                 David Fallon <davef@tetsubo.com>

use strict;

use vars qw(
  %FORM
  $userid
  $template
  $vars
);

use lib qw(.);

require "CGI.pl";

ConnectToDatabase();
confirm_login();

if (Param('enablequips') eq "off") {
    ThrowUserError("quips_disabled");
}
    
my $action = $::FORM{'action'} || "";

if ($action eq "show") {
    # Read in the entire quip list
    SendSQL("SELECT quip FROM quips");

    my @quips;
    while (MoreSQLData()) {
        my ($quip) = FetchSQLData();
        push(@quips, $quip);
    }

    $vars->{'quips'} = \@quips;
    $vars->{'show_quips'} = 1;
}

if ($action eq "add") {
    (Param('enablequips') eq "on") || ThrowUserError("no_new_quips");
    
    # Add the quip 
    my $comment = $::FORM{"quip"};
    $comment || ThrowUserError("need_quip");
    $comment !~ m/</ || ThrowUserError("no_html_in_quips");

    SendSQL("INSERT INTO quips (userid, quip) VALUES (". $userid . ", " . SqlQuote($comment) . ")");

    $vars->{'added_quip'} = $comment;
}

print "Content-type: text/html\n\n";
$template->process("list/quips.html.tmpl", $vars)
  || ThrowTemplateError($template->error());
