#!/usr/bonsaitools/bin/perl -w
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

require 'CGI.pl';

use strict;

LoadCheckins();

sub IsChecked {
    my ($value) = (@_);
    if ($value == $::BatchID) {
        return "CHECKED"
    } else {
        return ""
    }
}

print "Content-type: text/html

<HTML>
<TITLE>Let's do the time warp again...</TITLE>

Which hook would you like to see?
";

my @list;


foreach my $i (glob(DataDir() . "/batch-*\[0-9\].pl")) {
    if ($i =~ /batch-([0-9]*)\.pl/) {
        push(@list, $1);
    }
}

@list = sort {$b <=> $a} @list;

print '<FORM method=get action="';
if ($::FORM{'target'} eq 'showcheckins') {
print 'showcheckins.cgi';
} else {
print 'toplevel.cgi';
}
print '">
';
print "<INPUT TYPE=HIDDEN NAME=treeid VALUE=$::TreeID>\n";
print "<INPUT TYPE=SUBMIT Value=\"Submit\"><BR>\n";

my $value = shift(@list);

print "<INPUT TYPE=radio NAME=batchid VALUE=$value " . IsChecked($value). ">";
print "The current hook.<BR>\n";

my $count = 1;
foreach my $i (@list) {
    print "<INPUT TYPE=radio NAME=batchid VALUE=$i " . IsChecked($i) .
        ">\n";
    my $name = DataDir() . "/batch-$i.pl";
    require "$name";
    print "Hook for tree that closed on " . MyFmtClock($::CloseTimeStamp) .
        "<BR>\n";
}

print "<INPUT TYPE=SUBMIT Value=\"Submit\">\n";
print "</FORM>\n";

PutsTrailer();
