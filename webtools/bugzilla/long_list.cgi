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

# Shut up misguided -w warnings about "used only once":
use vars %::FORM;

print "Content-type: text/html\n\n";
print "<TITLE>Full Text Bug Listing</TITLE>\n";

my $generic_query  = "
select
  bugs.bug_id,
  bugs.product,
  bugs.version,
  bugs.rep_platform,
  bugs.op_sys,
  bugs.bug_status,
  bugs.bug_severity,
  bugs.priority,
  bugs.resolution,
  assign.login_name,
  report.login_name,
  bugs.component,
  bugs.bug_file_loc,
  bugs.short_desc
from bugs,profiles assign,profiles report
where assign.userid = bugs.assigned_to and report.userid = bugs.reporter and
";

ConnectToDatabase();

foreach my $bug (split(/:/, $::FORM{'buglist'})) {
    SendSQL("$generic_query bugs.bug_id = $bug");

    my @row;
    if (@row = FetchSQLData()) {
        my ($id, $product, $version, $platform, $opsys, $status, $severity,
            $priority, $resolution, $assigned, $reporter, $component, $url,
            $shortdesc) = (@row);
        print "<IMG SRC=\"1x1.gif\" WIDTH=1 HEIGHT=80 ALIGN=LEFT>\n";
        print "<TABLE WIDTH=100%>\n";
        print "<TD COLSPAN=4><TR><DIV ALIGN=CENTER><B><FONT =\"+3\">" .
            html_quote($shortdesc) .
                "</B></FONT></DIV>\n";
        print "<TR><TD><B>Bug#:</B> <A HREF=\"show_bug.cgi?id=$id\">$id</A>\n";
        print "<TD><B>Product:</B> $product\n";
        print "<TD><B>Version:</B> $version\n";
        print "<TD><B>Platform:</B> $platform\n";
        print "<TR><TD><B>OS/Version:</B> $opsys\n";
        print "<TD><B>Status:</B> $status\n";
        print "<TD><B>Severity:</B> $severity\n";
        print "<TD><B>Priority:</B> $priority\n";
        print "<TR><TD><B>Resolution:</B> $resolution</TD>\n";
        print "<TD><B>Assigned To:</B> $assigned\n";
        print "<TD><B>Reported By:</B> $reporter\n";
        print "<TR><TD><B>Component:</B> $component\n";
        print "<TR><TD COLSPAN=6><B>URL:</B> " . html_quote($url) . "\n";
        print "<TR><TD COLSPAN=6><B>Summary&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;:</B> " . html_quote($shortdesc) . "\n";
        print "<TR><TD><B>Description&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;:</B>\n</TABLE>\n";
        print "<PRE>" . html_quote(GetLongDescription($bug)) . "</PRE>\n";
        print "<HR>\n";
    }
}
