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

use diagnostics;
use strict;

use vars qw(
  %FORM
  $template
  $vars
);

use lib qw(.);

require "CGI.pl";

my $action = $::FORM{'action'} || "";

if ($action eq "show") {
    # Read in the entire quip list
    if (open (COMMENTS, "<data/comments")) {
        my @quips;
        push (@quips, $_) while (<COMMENTS>);        
        close COMMENTS;
        
        $vars->{'quips'} = \@quips;
        $vars->{'show_quips'} = 1;
    }
}

if ($action eq "add") {
    # Add the quip 
    my $comment = $::FORM{"quip"};
    if (!$comment) {
        DisplayError("Please enter a quip in the text field.");
        exit();
    }
    
    if ($comment =~ m/</) {
        DisplayError("Sorry - for security reasons, support for HTML tags has 
                      been turned off in quips.");
        exit();
    }

    open(COMMENTS, ">>data/comments");
    print COMMENTS $comment . "\n";
    close(COMMENTS);

    $vars->{'added_quip'} = $comment;
}

print "Content-type: text/html\n\n";
$template->process("info/quips.tmpl", $vars)
  || DisplayError("Template process failed: " . $template->error())
  && exit;
