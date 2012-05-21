#!/usr/local/bin/perl -w
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# Perl script to analyze the xpcom output file
#
# To create xpcom output file :
#
# setenv NSPR_LOG_MODULES nsComponentManager:5
# setenv NSPR_LOG_FILE xpcom.out
# ./mozilla
#
# Also to try to convert CID -> contractID this program looks for
# a file reg.out in the current directory. To generate this file
#
# $ regExport > reg.out
#
# Usage: analyze-xpcom-log.pl < xpcom.out
# [does better if ./reg.out is available]
#
# Suresh Duddi <dpsuresh@netscape.net>


use strict;

# forward declarations
sub getContractID($);
sub sum($);

# Configuration parameters
# Print all ?
my $all = 0;

# hash of cid -> contractid
my %contractid;
my %contractid_n;
my %failedContractid_n;

# count of instances of objects created
my (%objs, %objs_contractid, %failedObjs) = ();

# dlls loaded
my @dlls;

# temporaries
my ($cid, $n, $str);

while (<>) {
    chomp;

    # dlls loaded
    if (/loading \"(.*)\"/) {
        push @dlls, $1;
        next;
    }

    # FAILED ContractIDToClassID
    if (/ContractIDToClassID\((.*)\).*\[FAILED\]/) {
        $failedContractid_n{$1}++;
        next;
    }

    # ContractIDToClassID
    if (/ContractIDToClassID\((.*)\).*\{(.*)\}/) {
        $contractid{$2} = $1;
        $contractid_n{$2}++;
        next;
    }

    # CreateInstance()
    if (/CreateInstance\(\{(.*)\}\) succeeded/) {
        $objs{$1}++;
        next;
    }

    # CreateInstanceByContractID()
    if (/CreateInstanceByContractID\((.*)\) succeeded/) {
        $objs_contractid{$1}++;
        next;
    }

    # FAILED CreateInstance()
    if (/CreateInstance\(\{(.*)\}\) FAILED/) {
        $failedObjs{$1}++;
        next;
    }
}

# if there is a file named reg.out in the current dir
# then use that to fill in the ContractIDToClassID mapping.
my $REG;
open REG, "<reg.out";
while (<REG>) {
    chomp;
    if (/contractID -  (.*)$/) {
        my $id = $1;
        $cid = <REG>;
        chomp($cid);
        $cid =~ s/^.*\{(.*)\}.*$/$1/;
        $contractid{$cid} = $id;
    }
}

# print results
# ----------------------------------------------------------------------

# dlls loaded
print "dlls loaded [", scalar @dlls, "]\n";
print "----------------------------------------------------------------------\n";
for ($n = 0; $n < scalar @dlls; $n++) {
    printf "%2d. %s\n", $n+1, $dlls[$n];
}
print "\n";

# Objects created
print "Object creations from CID [", sum(\%objs), "]\n";
print "----------------------------------------------------------------------\n";
foreach $cid (sort {$objs{$b} <=> $objs{$a} } keys %objs) {
    last if (!$all && $objs{$cid} < 50);
    printf "%5d. %s - %s\n", $objs{$cid}, $cid, getContractID($cid);
}
print "\n";

print "Object creations from ContractID [", sum(\%objs_contractid), "]\n";
print "----------------------------------------------------------------------\n";
foreach $cid (sort {$objs_contractid{$b} <=> $objs_contractid{$a} } keys %objs_contractid) {
    last if (!$all && $objs_contractid{$cid} < 50);
    printf "%5d. %s - %s\n", $objs_contractid{$cid}, $cid, getContractID($cid);
}
print "\n";

# FAILED Objects created
print "FAILED Objects creations [", sum(\%failedObjs), "]\n";
print "----------------------------------------------------------------------\n";
foreach $cid (sort {$failedObjs{$b} <=> $failedObjs{$a} } keys %failedObjs) {
    last if (!$all && $failedObjs{$cid} < 50);
    printf "%5d. %s - %s", $failedObjs{$cid}, $cid, getContractID($cid);
}
print "\n";

# ContractIDToClassID calls
print "ContractIDToClassID() calls [", sum(\%contractid_n),"]\n";
print "----------------------------------------------------------------------\n";
foreach $cid (sort {$contractid_n{$b} <=> $contractid_n{$a} } keys %contractid_n) {
    last if (!$all && $contractid_n{$cid} < 50);
    printf "%5d. %s - %s\n", $contractid_n{$cid}, $cid, getContractID($cid);
}
print "\n";


# FAILED ContractIDToClassID calls
print "FAILED ContractIDToClassID() calls [", sum(\%failedContractid_n), "]\n";
print "----------------------------------------------------------------------\n";
foreach $cid (sort {$failedContractid_n{$b} <=> $failedContractid_n{$a} } keys %failedContractid_n) {
    last if (!$all && $failedContractid_n{$cid} < 50);
    printf "%5d. %s\n", $failedContractid_n{$cid}, $cid;
}
print "\n";


# Subroutines

sub getContractID($) {
    my $cid = shift;
    my $ret = "";
    $ret = $contractid{$cid} if (exists $contractid{$cid});
    return $ret;
}

sub sum($) {
    my $hash_ref = shift;
    my %hash = %$hash_ref;
    my $total = 0;
    my $key;
    foreach $key (keys %hash) {
        $total += $hash{$key};
    }
    return $total;
}
