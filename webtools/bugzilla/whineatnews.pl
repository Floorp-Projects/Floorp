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


# This is a script suitable for running once a day from a cron job.  It 
# looks at all the bugs, and sends whiny mail to anyone who has a bug 
# assigned to them that has status NEW that has not been touched for
# more than 7 days.

use diagnostics;
use strict;

require "globals.pl";

ConnectToDatabase();

SendSQL("select bug_id,login_name from bugs,profiles where " .
        "bug_status = 'NEW' and to_days(now()) - to_days(delta_ts) > " .
        Param('whinedays') . " and userid=assigned_to order by bug_id");

my %bugs;
my @row;

while (@row = FetchSQLData()) {
    my ($id, $email) = (@row);
    if (!defined $bugs{$email}) {
        $bugs{$email} = [];
    }
    push @{$bugs{$email}}, $id;
}


my $template = Param('whinemail');
my $urlbase = Param('urlbase');
my $emailsuffix = Param('emailsuffix');

foreach my $email (sort (keys %bugs)) {
    my %substs;
    $substs{'email'} = $email . $emailsuffix;
    $substs{'userid'} = $email;
    my $msg = PerformSubsts($template, \%substs);

    foreach my $i (@{$bugs{$email}}) {
        $msg .= "  ${urlbase}show_bug.cgi?id=$i\n"
    }

    my $sendmailparam = Param('sendmailnow') ? '' : "-ODeliveryMode=deferred";
    open SENDMAIL, "|/usr/lib/sendmail $sendmailparam -t"
        or die "Can't open sendmail";
    print SENDMAIL $msg;
    close SENDMAIL;
    print "$email      " . join(" ", @{$bugs{$email}}) . "\n";
}
