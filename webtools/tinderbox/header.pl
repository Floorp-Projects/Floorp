# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 


sub EmitHtmlTitleAndHeader {
    my($doctitle,$heading,$subheading) = @_;
    
    print "<HTML><HEAD><TITLE>$doctitle</TITLE>";
    print "<link rel=\"icon\" href=\"http://mozilla.org/images/mozilla-16.png\" type=\"image/png\">";
    print "</HEAD>";
    print "<BODY   BGCOLOR=\"#FFFFFF\" TEXT=\"#000000\"";
    print "LINK=\"#0000EE\" VLINK=\"#551A8B\" ALINK=\"#FF0000\">";

    if (open(BANNER, "<data/banner.html")) {
        while (<BANNER>) { print; }
        close BANNER;
    } elsif (open(BANNER, "<../bonsai/data/banner.html")) {
        while (<BANNER>) { print; }
        close BANNER;
    }

    print "<TABLE BORDER=0 CELLPADDING=12 CELLSPACING=0 WIDTH=\"100%\">";
    print " <TR>\n";
    print "  <TD>\n";
    print "   <TABLE BORDER=0 CELLPADDING=0 CELLSPACING=2>\n";
    print "    <TR><TD VALIGN=TOP ALIGN=CENTER NOWRAP>\n";
    print "     <FONT SIZE=\"+3\"><B><NOBR>$heading</NOBR></B></FONT>\n";
    print "    </TD></TR><TR><TD VALIGN=TOP ALIGN=CENTER>\n";
    print "     <B>$subheading</B>\n";
    print "    </TD></TR>\n";
    print "   </TABLE>\n";
    print "  </TD>\n";
    print "  <TD>\n";

    if (open(BLURB, "<data/blurb")) {
        while (<BLURB>) { print; }
        close BLURB;
    }

    print "</TD></TR></TABLE>\n";
}

sub EmitHtmlHeader {
    my($heading,$subheading) = @_;
    EmitHtmlTitleAndHeader($heading,$heading,$subheading);
}

1;
