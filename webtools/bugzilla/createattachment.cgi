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
#                 David Gardiner <david.gardiner@unisa.edu.au>

use diagnostics;
use strict;

require "CGI.pl";

use vars %::COOKIE, %::FILENAME;

confirm_login();

print "Content-type: text/html\n\n";

my $id = $::FORM{'id'};
die "invalid id: $id" unless $id=~/^\s*\d+\s*$/;

PutHeader("Create an attachment", "Create attachment", "Bug $id");


if (!defined($::FORM{'data'})) {
    print qq{
<form method=post ENCTYPE="multipart/form-data">
<input type=hidden name=id value=$id>
To attach a file to <a href="show_bug.cgi?id=$id">bug $id</a>, place it in a
file on your local machine, and enter the path to that file here:<br>
<input type=file name=data size=60>
<P>
Please also provide a one-line description of this attachment:<BR>
<input name=description size=60>
<BR>
What kind of file is this?
<br><input type=radio name=type value=patch>Patch file (text/plain, diffs)
<br><input type=radio name=type value="text/plain">Plain text (text/plain)
<br><input type=radio name=type value="text/html">HTML source (text/html)
<br><input type=radio name=type value="image/gif">GIF Image (image/gif)
<br><input type=radio name=type value="image/jpeg">JPEG Image (image/jpeg)
<br><input type=radio name=type value="image/png">PNG Image (image/png)
<br><input type=radio name=type value="application/octet-stream">Binary file (application/octet-stream)
<br><input type=radio name=type value="other">Other (enter mime type: <input name=othertype size=30>)
<P>
<input type=submit value="Submit">
</form>
<P>
};
} else {
    if ($::FORM{'data'} eq "" || !defined $::FILENAME{'data'}) {
        PuntTryAgain("No file was provided, or it was empty.");
    }
    my $desc = trim($::FORM{'description'});
    if ($desc eq "") {
        PuntTryAgain("You must provide a description of your attachment.");
    }
    my $ispatch = 0;
    my $mimetype = $::FORM{'type'};
    if (!defined $mimetype) {
        PuntTryAgain("You must select which kind of file you have.");
    }
    $mimetype = trim($mimetype);
    if ($mimetype eq "patch") {
        $mimetype = "text/plain";
        $ispatch = 1;
    }
    if ($mimetype eq "other") {
        $mimetype = $::FORM{'othertype'};
    }
    if ($mimetype !~ m@^(\w|-|\+|\.)+/(\w|-|\+|\.)+(;.*)?$@) {
        PuntTryAgain("You must select a legal mime type.  '<tt>" .
        html_quote($mimetype) . "</tt>' simply will not do.");
    }
    SendSQL("insert into attachments (bug_id, filename, description, mimetype, ispatch, submitter_id, thedata) values ($id," .
            SqlQuote($::FILENAME{'data'}) . ", " . SqlQuote($desc) . ", " .
            SqlQuote($mimetype) . ", $ispatch, " .
            DBNameToIdAndCheck($::COOKIE{'Bugzilla_login'}) . ", " .
            SqlQuote($::FORM{'data'}) . ")");
    SendSQL("select LAST_INSERT_ID()");
    my $attachid = FetchOneColumn();
    AppendComment($id, $::COOKIE{"Bugzilla_login"},
                  "Created an attachment (id=$attachid)\n$desc\n");

    print '<TABLE BORDER=1><TD><H2>Attachment <A TITLE="'.value_quote($desc).
      "\" HREF=\"showattachment.cgi?attach_id=$attachid\">$attachid</A> to bug $id created</H2>\n";
    system("./processmail", $id, $::COOKIE{'Bugzilla_login'});
    print "<TD><A HREF=\"show_bug.cgi?id=$id\">Go Back to BUG# $id</A></TABLE>\n";
    print "<P><A HREF=\"createattachment.cgi?id=$id\">Create another attachment to bug $id</A></P>\n";
}

PutFooter();

