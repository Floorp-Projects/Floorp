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

# Code derived from doeditowners.cgi


use diagnostics;
use strict;

require "CGI.pl";


# Shut up misguided -w warnings about "used only once":

use vars @::legal_product;


confirm_login();

print "Content-type: text/html\n\n";

# foreach my $i (sort(keys %::FORM)) {
#     print value_quote("$i $::FORM{$i}") . "<BR>\n";
# }

if (!UserInGroup("editcomponents")) {
    print "<H1>Sorry, you aren't a member of the 'editcomponents' group.</H1>\n";
    print "And so, you aren't allowed to edit the owners.\n";
    exit;
}


sub Check {
    my ($code1, $code2) = (@_);
    if ($code1 ne $code2) {
        print "<H1>A race error has occurred.</H1>";
        print "It appears that someone else has been changing the database\n";
        print "while you've been editing it.  I'm afraid you will have to\n";
        print "start all over.  Sorry! <P>\n";
        print "<p><a href=query.cgi>Go back to the query page</a>\n";
        exit;
    }
}


my @cmds;

sub DoOne {
    my ($oldvalue, $field, $where, $checkemail) = (@_);
    if (!defined $::FORM{$field}) {
        print "ERROR -- $field not defined!";
        exit;
    }
    if ($oldvalue ne $::FORM{$field}) {
        my $name = $field;
        $name =~ s/^.*-//;
        my $table = "products";
        if ($field =~ /^P\d+-C\d+-/) {
            $table = "components";
        }
        push @cmds, "update $table set $name=" .
            SqlQuote($::FORM{$field}) . " where $where";
        print "Changed $name for $where <P>";
        if ($checkemail) {
            DBNameToIdAndCheck($::FORM{$field});
        }
    }
}
            


PutHeader("Saving new component info");

unlink "data/versioncache";
GetVersionTable();

my $prodcode = "P000";

foreach my $product (@::legal_product) {
    SendSQL("select description, milestoneurl, disallownew from products where product='$product'");
    my @row = FetchSQLData();
    if (!@row) {
        next;
    }
    my ($description, $milestoneurl, $disallownew) = (@row);
    $prodcode++;
    Check($product, $::FORM{"prodcode-$prodcode"});

    my $where = "product=" . SqlQuote($product);
    DoOne($description, "$prodcode-description", $where);
    if (Param('usetargetmilestone')) {
        DoOne($milestoneurl, "$prodcode-milestoneurl", $where);
    }
    DoOne($disallownew, "$prodcode-disallownew", $where);

    SendSQL("select value, initialowner, initialqacontact, description from components where program=" . SqlQuote($product) . " order by value");
    my $c = 0;
    while (my @row = FetchSQLData()) {
        my ($component, $initialowner, $initialqacontact, $description) =
            (@row);
        $c++;
        my $compcode = $prodcode . "-" . "C$c";

	Check($component, $::FORM{"compcode-$compcode"});

        my $where = "program=" . SqlQuote($product) . " and value=" .
            SqlQuote($component);

        DoOne($initialowner, "$compcode-initialowner", $where, 1);
        if (Param('useqacontact')) {
            DoOne($initialqacontact, "$compcode-initialqacontact", $where,
                  1);
        }
        DoOne($description, "$compcode-description", $where);
    }

}

print "Saving changes.<P>\n";

foreach my $cmd (@cmds) {
    print "$cmd <BR>";
    SendSQL($cmd);
}

unlink "data/versioncache";

print "OK, done.<p>\n";
print "<a href=editcomponents.cgi>Edit the components some more.</a><p>\n";
print "<a href=query.cgi>Go back to the query page.</a>\n";
