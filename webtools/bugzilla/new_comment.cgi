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

my %FORM;
my $buffer = "";
if ($ENV{'REQUEST_METHOD'} eq "GET") { $buffer = $ENV{'QUERY_STRING'}; }
else { read(STDIN, $buffer, $ENV{'CONTENT_LENGTH'}); }
# Split the name-value pairs
my @pairs = split(/&/, $buffer);
foreach my $pair (@pairs)
{
    my ($name, $value) = split(/=/, $pair);

    $value =~ tr/+/ /;
    $value =~ s/^(\s*)//s;
    $value =~ s/(\s*)$//s;
    $value =~ s/%([a-fA-F0-9][a-fA-F0-9])/pack("C", hex($1))/eg;
    $FORM{$name} = $value;
}
my $c = $FORM{"comment"};
if ( (!defined $c) || ($c eq '') ) {
    print "Content-type: text/html\n\n";
    print "<TITLE>Nothing on your mind?</TITLE>";
    print "<H1>Does your mind draw a blank?</H1>";
	print "<H2> Hit back, and try again...</H2>";
    exit 0;
}
if ($c =~ m/</) {
	print "Content-type: text/html\n\n";
	print "<CENTER><H1>For security reasons, support for tags";
	print " has been turned off in quips.\n</H1>\n";
	print "<H2> Hit back, and try again...</H2></CENTER>\n";
    exit 0;
}

open(COMMENTS, ">>data/comments");
print COMMENTS $FORM{"comment"} . "\n";
close(COMMENTS);
print "Content-type: text/html\n\n";
print "<TITLE>The Word Of Confirmation</TITLE>";
print "<H1>Done</H1>";
print $c;
