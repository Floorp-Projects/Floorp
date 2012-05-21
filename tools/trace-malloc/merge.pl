#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


$argv = $ARGV[0];
open( bloatFile, $argv ) or die "Can't open $argv: $!\n";
while (<bloatFile>) {
    if (/name=/) {
        ($td,$name,$func,$a,$ntd) = split(/>/, $_);
        ($fname, $memSize) = split( /&nbsp;/, $func );
        ($trash, $linkName) = split( /"/, $name );
        $namesLinks{$fname} = $linkName;
        if ($namesSizes{$fname}) {
            $namesSizes{$fname} = $namesSizes{$fname} + $memSize;
        }
        else {
            $namesSizes{$fname} = $memSize;
        }
    }
}

$argv = $ARGV[1];
if ($argv)
{
    open( bloatFile, $argv ) or die "Can't open $argv: $!\n";
    while (<bloatFile>) {
        if (/name=/) {
            ($td,$name,$func,$a,$ntd) = split(/>/, $_);
            ($fname, $memSize) = split( /&nbsp;/, $func );
            $namesSizes{$fname} = $namesSizes{$fname} - $memSize;
        }
    }
}

sub byvalue { $namesSizes{$b} <=> $namesSizes{$a} }


print '<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0//EN">';
print "\n<html><head>\n";
print "<title>Bloat Blame Delta</title>\n";
print '<link rel="stylesheet" type="text/css" href="blame.css">';
print "\n</head>\n<body>\n<table>\n";
print "<thead><tr><td>Memory Allocated</td><td>Function Name</td><td>Link</td></tr></thead>\n";

foreach $key (sort byvalue keys %namesSizes) {
    if ($namesSizes{$key}) 
    {
        print "<tr>\n";
        print '  <td>', $namesSizes{$key},"</td>\n"; 
        print "  <td> <a href=\"$ARGV[0]#$namesLinks{$key}\">", $key,  "</a></td>\n";
        print "</tr>\n"
    }
}

print "</table>\n</body></html>";
