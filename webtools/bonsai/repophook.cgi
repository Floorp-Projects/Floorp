#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

require 'CGI.pl';

use diagnostics;
use strict;

print "Content-type: text/html

<HTML>";

CheckPassword($::FORM{'password'});

my $startfrom = ParseTimeAndCheck(FormData('startfrom'));

Lock();
LoadTreeConfig();
LoadCheckins();
@::CheckInList = ();
WriteCheckins();
Unlock();


$| = 1;

print "<TITLE> Rebooting, please wait...</TITLE>

<H1>Recreating the hook</H1>

<h3>$::TreeInfo{$::TreeID}->{'description'}</h3>

<p>
Searching for first checkin after " . MyFmtClock($startfrom) . "...<p>\n";


my $mungedname = $::TreeInfo{$::TreeID}->{'repository'};
$mungedname =~ s@/@_@g;
$mungedname =~ s/^_//;

my $filename = "data/checkinlog/$mungedname";

open(FID, "<$filename") || die "Can't open $filename";

my $foundfirst = 0;

my $buffer = "";


my $tempfile = "data/repophook.$$";

my $count = 0;
my $lastdate = 0;

sub FlushBuffer {
    if (!$foundfirst || $buffer eq "") {
        return;
    }
    open(TMP, ">$tempfile") || die "Can't open $tempfile";
    print TMP "junkline\n\n$buffer\n";
    close(TMP);
    system("./addcheckin.pl -treeid $::TreeID $tempfile");
#    unlink($tempfile);
    $buffer = "";
    $count++;
    if ($count % 100 == 0) {
        print "$count scrutinized...<br>\n";
    }
}

my $now = time();

while (<FID>) {
    chomp();
    my $line = $_;
    if ($line =~ /^.\|/) {
        my ($chtype, $date) = (split(/\|/, $line));
        if ($date < $lastdate) {
            print "Ick; dates out of order!<br>\n";
            print "<pre>" . value_quote($line) . "</pre><p>\n";
        }
        $lastdate = $date;
        if ($foundfirst) {
            $buffer .= "$line\n";
        } else {
            if ($date >= $startfrom) {
                if ($date >= $now) {
                    print "Found a future date! (ignoring):<br>\n";
                    print "<pre>" . value_quote($line) . "</pre><p>\n";
                } else {
                    $foundfirst = 1;
                    print "Found first line: <br>\n";
                    print "<pre>" . value_quote($line) . "</pre><p>\n";
                    print "OK, now processing checkins...<p>";
                    $buffer = "$line\n";
                    $count = 0;
                }
            } else {
                $count++;
                if ($count % 2000 == 0) {
                    print "Skipped $count lines...<p>\n";
                }
            }
        }
    } elsif ($line =~ /^:ENDLOGCOMMENT$/) {
        $buffer .= "$line\n";
        FlushBuffer();
    } else {
        $buffer .= "$line\n";
    }
}

FlushBuffer();
        
                
print "OK, done. \n";

PutsTrailer();
