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
require "defparams.pl";

# Shut up misguided -w warnings about "used only once":
use vars @::param_desc,
    @::param_list,
    %::COOKIE;

confirm_login();

print "Content-type: text/html\n\n";

if (Param("maintainer") ne $::COOKIE{Bugzilla_login}) {
    print "<H1>Sorry, you aren't the maintainer of this system.</H1>\n";
    print "And so, you aren't allowed to edit the parameters of it.\n";
    exit;
}


PutHeader("Edit parameters");

print "This lets you edit the basic operating parameters of bugzilla.\n";
print "Be careful!\n";
print "<p>\n";
print "Any item you check Reset on will get reset to its default value.\n";

print "<FORM METHOD=\"POST\" ACTION=\"doeditparams.cgi\">\n<TABLE>\n";

my $rowbreak = "<TR><TD COLSPAN=\"2\"><HR></TD></TR>";
print $rowbreak;

foreach my $i (@::param_list) {
    print "<TR><TH ALIGN=\"RIGHT\" VALIGN=\"TOP\">$i:</TH><TD>$::param_desc{$i}</TD></TR>\n";
    print "<TR><TD VALIGN=\"TOP\"><INPUT TYPE=\"checkbox\" NAME=\"reset-$i\">Reset</TD><TD>\n";
    my $value = Param($i);
    SWITCH: for ($::param_type{$i}) {
	/^t$/ && do {
            print "<INPUT SIZE=\"80\" NAME=\"$i\" VALUE=\"" .
                value_quote($value) . '">\n';
            last SWITCH;
	};
	/^l$/ && do {
            print "<TEXTAREA WRAP=\"HARD\" NAME=\"$i\" ROWS=\"10\" COLS=\"80\">" .
                value_quote($value) . "</textarea>\n";
            last SWITCH;
	};
        /^b$/ && do {
            my $on;
            my $off;
            if ($value) {
                $on = "checked";
                $off = "";
            } else {
                $on = "";
                $off = "checked";
            }
            print "<INPUT TYPE=\"radio\" NAME=\"$i\" VALUE=\"1\" $on>On\n";
            print "<INPUT TYPE=\"radio\" NAME=\"$i\" VALUE=\"0\" $off>Off\n";
            last SWITCH;
        };
        # DEFAULT
        print "<FONT COLOR=\"red\"><BLINK>Unknown param type $::param_type{$i}!!!</BLINK></FONT>\n";
    }
    print "</td></tr>\n";
    print $rowbreak;
}

print "<TR><TH ALIGN=\"RIGHT\" VALIGN=\"TOP\">version:</TH><TD>
What version of Bugzilla this is.  This can't be modified here, but
<tt>%version%</tt> can be used as a parameter in places that understand
such parameters</td></tr>
<tr><td></td><td>" . Param('version') . "</td></tr>";

print "</table>\n";

print "<INPUT TYPE=\"reset\" VALUE=\"Reset form\"><BR>\n";
print "<INPUT TYPE=\"submit\" VALUE=\"Submit changes\">\n";

print "</form>\n";

print "<P><A HREF=\"query.cgi\">Skip all this, and go back to the query page</A>\n";
