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


use strict;

require "CGI.pl";
require "defparams.pl";

# Shut up misguided -w warnings about "used only once":
use vars @::param_desc,
    @::param_list;

print "Content-type: text/html\n\n";

#    
# Check to ensure they've logged in properly
#    

unless ((defined($::FORM{password})) && (CheckPassword($::FORM{password}))) {
    print "<form method=post action=editparams.cgi>\n";
    print "<h2>Sorry, you must enter a password to see Bonsai parameters.</h2>\n";
    print "<hr><b>Enter password to access parameters:</b> <input name=password type=password> <br>";
    print "<input type=submit value=\"Enter Password\">\n";
    print "</form>\n";
    print "<hr>\n";
    exit;
}   

#         
# We're verified, now bring everything up
#    

PutsHeader("Edit parameters");

print "This lets you edit the basic operating parameters of bonsai.\n";
print "Be careful!\n";
print "<p>\n";
print "Any item you check Reset on will get reset to its default value.\n";

print "<form method=post action=doeditparams.cgi><table>\n";

my $rowbreak = "<tr><td colspan=2><hr></td></tr>";
print $rowbreak;

foreach my $i (@::param_list) {
    print "<tr><th align=right valign=top>$i:</th><td>$::param_desc{$i}</td></tr>\n";
    print "<tr><td valign=top><input type=checkbox name=reset-$i>Reset</td><td>\n";
    my $value = Param($i);
    SWITCH: for ($::param_type{$i}) {
	/^t$/ && do {
            print "<input size=80 name=$i value=\"" .
                value_quote($value) . "\">\n";
            last SWITCH;
	};
        /^p/ && do {
            print "<input size=80 type=password name=$i value=\"" .
                value_quote($value) . "\">\n";
            last SWITCH;
        };
        /^l$/ && do {
            print "<textarea wrap=hard name=$i rows=10 cols=80>" .
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
            print "<input type=radio name=$i value=1 $on>On\n";
            print "<input type=radio name=$i value=0 $off>Off\n";
            last SWITCH;
        };
        # DEFAULT
        print "<font color=red><blink>Unknown param type $::param_type{$i}!!!</blink></font>\n";
    }
    print "</td></tr>\n";
    print $rowbreak;
}

print "<tr><th align=right valign=top>version:</th><td>
What version of Bonsai this is.  This can't be modified here, but
<tt>%version%</tt> can be used as a parameter in places that understand
such parameters</td></tr>
<tr><td></td><td>" . Param('version') . "</td></tr>";

print "</table>\n";

print "<hr><b>Enter password to change parameters:</b>
       <input name=password type=password> <br>";
print "<input type=reset value=\"Reset form\"><br>\n";
print "<input type=submit value=\"Submit changes\">\n";

print "</form>\n";

print "<p><a href=toplevel.cgi>Skip all this, and go back to the main bonsai page</a>\n";
