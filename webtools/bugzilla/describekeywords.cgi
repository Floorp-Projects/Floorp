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
# The Initial Developer of the Original Code is Terry Weissman.
# Portions created by Terry Weissman are
# Copyright (C) 2000 Terry Weissman. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>

use diagnostics;
use strict;

require "CGI.pl";

ConnectToDatabase();

print "Content-type: text/html\n\n";

PutHeader("Bugzilla keyword description");

print qq{
<TABLE BORDER=1 CELLPADDING=4 CELLSPACING=0>
<TR BGCOLOR="#6666FF">
<TH ALIGN="left">Name</TH>
<TH ALIGN="left">Description</TH>
<TH ALIGN="left">Bugs</TH>
</TR>
};

SendSQL("SELECT keyworddefs.name, keyworddefs.description, 
                COUNT(keywords.bug_id)
         FROM keyworddefs LEFT JOIN keywords ON keyworddefs.id=keywords.keywordid
         GROUP BY keyworddefs.id
         ORDER BY keyworddefs.name");

while (MoreSQLData()) {
    my ($name, $description, $bugs) = FetchSQLData();
    if ($bugs) {
        my $q = url_quote($name);
        $bugs = qq{<A HREF="buglist.cgi?keywords=$q">$bugs</A>};
    } else {
        $bugs = "none";
    }
    print qq{
<TR>
<TH>$name</TH>
<TD>$description</TD>
<TD ALIGN="right">$bugs</TD>
</TR>
};
}

print "</TABLE><P>\n";

quietly_check_login();

if (UserInGroup("editkeywords")) {
    print "<p><a href=editkeywords.cgi>Edit keywords</a><p>\n";
}

navigation_header();
