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
# Contributor(s): Terry Weissman <terry@mozilla.org>,
#                 Harrison Page <harrison@netscape.com>
#         Gervase Markham <gerv@gerv.net>

# Run me out of cron at midnight to collect Bugzilla statistics.


use AnyDBM_File;
use strict;
use vars @::legal_product;

require "globals.pl";

# tidy up after graphing module
if (chdir("graphs")) {
    unlink <./*.gif>;
    unlink <./*.png>;
    chdir("..");
}

ConnectToDatabase(1);
GetVersionTable();

my @myproducts;
push( @myproducts, "-All-", @::legal_product );

foreach (@myproducts) {
    my $dir = "data/mining";

    &check_data_dir ($dir);
    &collect_stats ($dir, $_);
}

&calculate_dupes();

# Generate a static RDF file containing the default view of the duplicates data.
open(CGI, "REQUEST_METHOD=GET QUERY_STRING=ctype=rdf ./duplicates.cgi |")
  || die "can't fork duplicates.cgi: $!";
open(RDF, ">data/duplicates.tmp")
  || die "can't write to data/duplicates.tmp: $!";
my $headers_done = 0;
while (<CGI>) {
  print RDF if $headers_done;
  $headers_done = 1 if $_ eq "\n";
}
close CGI;
close RDF;
if (-s "data/duplicates.tmp") {
    rename("data/duplicates.rdf", "data/duplicates-old.rdf");
    rename("data/duplicates.tmp", "data/duplicates.rdf");
}

sub check_data_dir {
    my $dir = shift;

    if (! -d) {
        mkdir $dir, 0777;
        chmod 0777, $dir;
    }
}

sub collect_stats {
    my $dir = shift;
    my $product = shift;
    my $when = localtime (time);
    my $product_id = get_product_id($product) unless $product eq '-All-';

    die "Unknown product $product" unless ($product_id or $product eq '-All-');

    # NB: Need to mangle the product for the filename, but use the real
    # product name in the query
    my $file_product = $product;
    $file_product =~ s/\//-/gs;
    my $file = join '/', $dir, $file_product;
    my $exists = -f $file;

    if (open DATA, ">>$file") {
        push my @row, &today;

        foreach my $status ('NEW', 'ASSIGNED', 'REOPENED', 'UNCONFIRMED', 'RESOLVED', 'VERIFIED', 'CLOSED') {
            if( $product eq "-All-" ) {
                SendSQL("select count(bug_status) from bugs where bug_status='$status'");
            } else {
                SendSQL("select count(bug_status) from bugs where bug_status='$status' and product_id=$product_id");
            }

            push @row, FetchOneColumn();
        }

        foreach my $resolution ('FIXED', 'INVALID', 'WONTFIX', 'LATER', 'REMIND', 'DUPLICATE', 'WORKSFORME', 'MOVED') {
            if( $product eq "-All-" ) {
                SendSQL("select count(resolution) from bugs where resolution='$resolution'");
            } else {
                SendSQL("select count(resolution) from bugs where resolution='$resolution' and product_id=$product_id");
            }

            push @row, FetchOneColumn();
        }

        if (! $exists) {
            print DATA <<FIN;
# Bugzilla Daily Bug Stats
#
# Do not edit me! This file is generated.
#
# fields: DATE|NEW|ASSIGNED|REOPENED|UNCONFIRMED|RESOLVED|VERIFIED|CLOSED|FIXED|INVALID|WONTFIX|LATER|REMIND|DUPLICATE|WORKSFORME|MOVED
# Product: $product
# Created: $when
FIN
        }

        print DATA (join '|', @row) . "\n";
        close DATA;
    } else {
        print "$0: $file, $!";
    }
}

sub calculate_dupes {
    SendSQL("SELECT * FROM duplicates");

    my %dupes;
    my %count;
    my @row;
    my $key;
    my $changed = 1;

    my $today = &today_dash;

    # Save % count here in a date-named file
    # so we can read it back in to do changed counters
    # First, delete it if it exists, so we don't add to the contents of an old file
    if (my @files = <data/duplicates/dupes$today*>) {
        unlink @files;
    }
   
    dbmopen(%count, "data/duplicates/dupes$today", 0644) || die "Can't open DBM dupes file: $!";

    # Create a hash with key "a bug number", value "bug which that bug is a
    # direct dupe of" - straight from the duplicates table.
    while (@row = FetchSQLData()) {
        my $dupe_of = shift @row;
        my $dupe = shift @row;
        $dupes{$dupe} = $dupe_of;
    }

    # Total up the number of bugs which are dupes of a given bug
    # count will then have key = "bug number", 
    # value = "number of immediate dupes of that bug".
    foreach $key (keys(%dupes)) 
    {
        my $dupe_of = $dupes{$key};

        if (!defined($count{$dupe_of})) {
            $count{$dupe_of} = 0;
        }

        $count{$dupe_of}++;
    }   

    # Now we collapse the dupe tree by iterating over %count until
    # there is no further change.
    while ($changed == 1)
    {
        $changed = 0;
        foreach $key (keys(%count)) {
            # if this bug is actually itself a dupe, and has a count...
            if (defined($dupes{$key}) && $count{$key} > 0) {
                # add that count onto the bug it is a dupe of,
                # and zero the count; the check is to avoid
                # loops
                if ($count{$dupes{$key}} != 0) {
                    $count{$dupes{$key}} += $count{$key};
                    $count{$key} = 0;
                    $changed = 1;
                }
            }
        }
    }

    # Remove the values for which the count is zero
    foreach $key (keys(%count))
    {
        if ($count{$key} == 0) {
            delete $count{$key};
        }
    }
   
    dbmclose(%count);
}

sub today {
    my ($dom, $mon, $year) = (localtime(time))[3, 4, 5];
    return sprintf "%04d%02d%02d", 1900 + $year, ++$mon, $dom;
}

sub today_dash {
    my ($dom, $mon, $year) = (localtime(time))[3, 4, 5];
    return sprintf "%04d-%02d-%02d", 1900 + $year, ++$mon, $dom;
}

