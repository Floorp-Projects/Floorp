#!/usr/bonsaitools/bin/perl -w
#  cvsgraph.cgi -- a graph of all branchs, tags, etc.

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
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Jacob Steenhagen
#
# Contributor(s): Jacob Steenhagen <jake@acutex.net>

use diagnostics;
use strict;

require 'CGI.pl';

# This cgi doesn't actually generate a graph.  It relies on the cvsgraph
# program found at http://www.akhphd.au.dk/~bertho/cvsgraph/
# File locations can be set at in the editparams.cgi page.

sub print_top {
    my ($title) = @_;
    $title ||= "Error";

    print "Content-type: text/html\n\n";
    print "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n";
    print "<html>\n<head>\n";
    print " <title>$title</title>\n";
    print "</head>\n<body>\n";
}

sub print_bottom {
    print "</body>\n</html>\n";
}

sub print_usage {
    &print_top;
    print "This script requires a filename.\n";
    &print_bottom;
}

### Live code below

unless (Param('cvsgraph')) {
    &print_top;
    print "CVS Graph is not currently configured for this installation\n";
    &print_bottom;
    exit;
}

my @src_roots = getRepositoryList();

# Handle the "file" argument
#
my $filename = '';
$filename = $::FORM{file} if defined $::FORM{file};
if ($filename eq '') {
    &print_usage;
    exit;
}

my ($file_head, $file_tail) = $filename =~ m@(.*/)?(.+)@;

# Handle the "root" argument
#
my $root = $::FORM{root};
if (defined $root and $root ne '') {
    $root =~ s|/$||;
    validateRepository($root);
    if (-d $root) {
        unshift(@src_roots, $root);
    } else {
        &print_top;
        print "Error:  Root, $root, is not a directory.<BR><BR>\n";
        &print_bottom;
        exit;
    }
}

# Find the rcs file
#
my $rcs_filename;
my $found_rcs_file = 0;
foreach (@src_roots) {
    $root = $_;
    $rcs_filename = "$root/$filename,v";
    $rcs_filename = Fix_BonsaiLink($rcs_filename);
    $found_rcs_file = 1, last if -r $rcs_filename;
    $rcs_filename = "$root/${file_head}Attic/$file_tail,v";
    $found_rcs_file = 1, last if -r $rcs_filename;
}

unless ($found_rcs_file) {
    &print_top;
    print "Rcs file, $filename, does not exist.\n";
    print "<pre>rcs_filename => '$rcs_filename'\nroot => '$root'</pre>\n";
    &print_bottom;
    exit;
}

# Hack these variables up the way that the cvsgraph executable wants them
$rcs_filename =~ s:^$root/::;
my $conf = $0;
$conf =~ s:[^/\\]+$::;
$conf .= "data/cvsgraph.conf";

my @cvsgraph_cmd = (Param("cvsgraph"),
                    "-c", $conf,
                    "-r", $root);

if (defined $::FORM{'image'}) {
    print "Content-type: image/png\n\n";
}
else {
    push(@cvsgraph_cmd, "-i", "-M", "revmap"); # Include args to make map
    &print_top("CVS Graph for " . $file_tail);
}

system(@cvsgraph_cmd, $rcs_filename);

if (!defined $::FORM{'image'}) {
    print qq{<img src="cvsgraph.cgi?file=$::FORM{'file'}&image=1" };
    print qq{usemap="#revmap" alt="$filename" border="0">};
    &print_bottom;
}
