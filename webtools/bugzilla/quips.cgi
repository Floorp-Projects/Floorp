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
# Contributor(s): Owen Taylor <otaylor@redhat.com>

use diagnostics;
use strict;
use vars ( %::FORM );

require "CGI.pl";

print "Content-type: text/html\n\n";

PutHeader("Quips for the impatient", "Add your own clever headline");

print qq{
The buglist picks a random quip for the headline, and 
you can extend the quip list.  Type in something clever or
funny or boring and bonk on the button.

<FORM METHOD=POST ACTION="new_comment.cgi">
<INPUT SIZE=80 NAME="comment"><BR>
<INPUT TYPE="submit" VALUE="Add This Quip">
</FORM>
};

if (exists $::FORM{show_quips}) {

    print qq{
<H2>Existing headlines</H2>
};

    if (open (COMMENTS, "<data/comments")) {
        while (<COMMENTS>) {
            print $_,"<br>\n";
        }
        close COMMENTS;
    }
    print "<P>";
} else {
    print qq{
For the impatient, you can 
<A HREF="quips.cgi?show_quips=yes">view the whole quip list</A>.
};
    print "<P>";
}

PutFooter();
