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
# Contributor(s): Sam Ziegler <sam@ziegler.org>
# Terry Weissman <terry@mozilla.org>

# Code derived from editparams.cgi, editowners.cgi

use diagnostics;
use strict;

require "CGI.pl";

# Shut up misguided -w warnings about "used only once":

use vars @::legal_product;

confirm_login();

print "Content-type: text/html\n\n";

if (!UserInGroup("editcomponents")) {
    print "<H1>Sorry, you aren't a member of the 'editcomponents' group.</H1>\n";
    print "And so, you aren't allowed to edit the owners.\n";
    exit;
}


PutHeader("Edit Components");

print "This lets you edit the program components of bugzilla.\n";

print "<form method=post action=doeditcomponents.cgi>\n";

my $rowbreak = "<tr><td colspan=2><hr></td></tr>";

unlink "data/versioncache";
GetVersionTable();

my $prodcode = "P0";

foreach my $product (@::legal_product) {
    SendSQL("select description, milestoneurl, disallownew from products where product='$product'");
    my @row = FetchSQLData();
    if (!@row) {
        next;
    }
    my ($description, $milestoneurl, $disallownew) = (@row);
    $prodcode++;
    print "<input type=hidden name=prodcode-$prodcode value=\"" .
        value_quote($product) . "\">\n";
    print "<table><tr><th align=left valign=top>$product</th><td></td></tr>\n";
    print "<tr><th align=right>Description:</th>\n";
    print "<td><input size=80 name=$prodcode-description value=\"" .
        value_quote($description) . "\"></td></tr>\n";
    if (Param('usetargetmilestone')) {
        print "<tr><th align=right>MilestoneURL:</th>\n";
        print "<td><input size=80 name=$prodcode-milestoneurl value=\"" .
            value_quote($milestoneurl) . "\"></td></tr>\n";
    }
    my $check0 = !$disallownew ? " SELECTED" : "";
    my $check1 = $disallownew ? " SELECTED" : "";
    print "<tr><td colspan=2><select name=$prodcode-disallownew>\n";
    print "<option value=0$check0>Open; new bugs may be submitted against this project\n";
    print "<option value=1$check1>Closed; no new bugs may be submitted against this project\n";
    print "</select></td></tr>\n";

    print "<tr><td colspan=2>Components:</td></tr></table>\n";
    print "<table>\n";
    
    SendSQL("select value, initialowner, initialqacontact, description from components where program=" . SqlQuote($product) . " order by value");
    my $c = 0;
    while (my @row = FetchSQLData()) {
        my ($component, $initialowner, $initialqacontact, $description) =
            (@row);
        $c++;
        my $compcode = $prodcode . "-" . "C$c";
        print "<input type=hidden name=compcode-$compcode value=\"" .
            value_quote($component) . "\">\n";
        print "<tr><th>$component</th><th align=right>Description:</th>\n";
        print "<td><input size=80 name=$compcode-description value=\"" .
        value_quote($description) . "\"></td></tr>\n";
        print "<tr><td></td><th align=right>Initial owner:</th>\n";
        print "<td><input size=60 name=$compcode-initialowner value=\"" .
        value_quote($initialowner) . "\"></td></tr>\n";
        if (Param('useqacontact')) {
            print "<tr><td></td><th align=right>Initial QA contact:</th>\n";
            print "<td><input size=60 name=$compcode-initialqacontact value=\"" .
                value_quote($initialqacontact) . "\"></td></tr>\n";
        }
    }

    print "</table><hr>\n";

}

print "<input type=submit value=\"Submit changes\">\n";

print "</form>\n";

print "<p><a href=query.cgi>Skip all this, and go back to the query page</a>\n";
