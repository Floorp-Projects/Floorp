#!/usr/bin/perl -w
######################################
# Infobot Factoid Import/Export Tool #
######################################

use strict;
use AnyDBM_File;
use Fcntl;

if (not @ARGV == 2) {
    &use();
} else {
    my $command = shift @ARGV;
    my $filename = shift @ARGV;
    if ($command eq '-d') {
        &dump($filename);
    } elsif ($command eq '-i') {
        &import($filename);
    } else {
        &use();
    }
}

sub use {
    print "\n";
    print "  usage:  $0 -d dbname\n";
    print "          prints out an ascii flat file of the database listed.\n";
    print "          dbname should be the basename of the db, e.g.\n";
    print "            $0 -d ../factoids-is > is.fact\n";
    print "            $0 -d ../factoids-are > are.fact\n";
    print "\n";
    print "          $0 -i dbname\n";
    print "          imports an ascii flat file into the database listed.\n";
    print "          dbname should be the basename of the db, e.g.\n";
    print "            $0 -i ../factoids-is < chemicals.fact\n";
    print "            $0 -i ../factoids-is < is.fact\n";
    print "            $0 -i ../factoids-are < are.fact\n";
    print "\n";
    exit(1);
}

sub dump {
    my %db;
    tie(%db, 'AnyDBM_File', shift, O_RDONLY, 0666);
    while (my ($key, $val) = each %db) {
        chomp $val;
        print "$key => $val\n";
    }
}

sub import {
    my %db;
    tie(%db, 'AnyDBM_File', shift, O_WRONLY|O_CREAT, 0666);
    while (<STDIN>) {
        chomp;
        unless (m/\s*(.+?)\s+=(?:is=|are=)?>\s+(.+?)\s*$/o) {
            m/\s*(.+?)\s+(?:is|are)?\s+(.+?)\s*$/o;
        }
        if (length($1) and length($2)) {
            if (defined($db{$1})) {
                if (not $db{$1} =~ m/^(|.*\|)\Q$2\E(|.*\|)$/s) {
                    $db{$1} .= "|$2";
                }
            } else {
                $db{$1} = $2;
            }
        }
    }
}
