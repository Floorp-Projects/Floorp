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

require "CGI.pl";

print "Content-type: text/html\n\n";

PutHeader("Changes made to bug $::FORM{'id'}", "Activity log",
          "Bug $::FORM{'id'}");

my $query = "
        select bugs_activity.field, bugs_activity.when,
                bugs_activity.oldvalue, bugs_activity.newvalue,
                profiles.login_name
        from bugs_activity,profiles
        where bugs_activity.bug_id = $::FORM{'id'}
        and profiles.userid = bugs_activity.who
        order by bugs_activity.when";

ConnectToDatabase();
SendSQL($query);

print "<table border cellpadding=4>\n";
print "<tr>\n";
print "    <th>Who</th><th>What</th><th>Old value</th><th>New value</th><th>When</th>\n";
print "</tr>\n";

my @row;
while (@row = FetchSQLData()) {
    my ($field,$when,$old,$new,$who) = (@row);
    $old = value_quote($old);
    $new = value_quote($new);
    print "<tr>\n";
    print "<td>$who</td>\n";
    print "<td>$field</td>\n";
    print "<td>$old</td>\n";
    print "<td>$new</td>\n";
    print "<td>$when</td>\n";
    print "</tr>\n";
}
print "</table>\n";
print "<hr><a href=show_bug.cgi?id=$::FORM{'id'}>Back to bug $::FORM{'id'}</a>\n";
