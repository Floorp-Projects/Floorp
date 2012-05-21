#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Use high resolution routines if installed (on win32 or linux), using
# eval as try/catch block around import of modules. Otherwise, just use 'time()'.
#
# 'Win32::API'  <http://www.activestate.com/PPMPackages/zips/5xx-builds-only/Win32-API.zip>
# 'Time::HiRes' <http://search.cpan.org/search?dist=Time-HiRes> 
#   (also: http://rpmfind.net/linux/rpm2html/search.php?query=perl-Time-HiRes)
#
package Time::PossiblyHiRes;

use strict;

#use Time::HiRes qw(gettimeofday);

my $getLocalTime;                      # for win32
my $lpSystemTime = pack("SSSSSSSS");   # for win32
my $timesub;                           # code ref

# returns 12 char string "'s'x9.'m'x3" which is milliseconds since epoch, 
# although resolution may vary depending on OS and installed packages

sub getTime () {

    return &$timesub 
	if $timesub;

    $timesub = sub { time() . "000"; }; # default

    return &$timesub 
	if $^O eq "MacOS"; # don't know a better way on Mac

    if ($^O eq "MSWin32") {
	eval "use Win32::API;";
	$timesub = sub { 
	    # pass pointer to struct, void return 
	    $getLocalTime = 
		eval "new Win32::API('kernel32', 'GetLocalTime', [qw{P}], qw{V});"
		    unless $getLocalTime;
	    $getLocalTime->Call($lpSystemTime);
	    my @t = unpack("SSSSSSSS", $lpSystemTime);
	    sprintf("%9s%03s", time(), pop @t);
	} if !$@;
    } 

    # ass-u-me if not mac/win32, then we're on a unix flavour
    else {
	eval "use Time::HiRes qw(gettimeofday);";
	$timesub = sub { 
	    my @t = gettimeofday();
	    $t[0]*1000 + int($t[1]/1000);	    
	} if !$@;
    }

    return &$timesub;

} 

#
#
# Test script to compare with low-res time:
#
# require "gettime.pl";
#
# use POSIX qw(strftime);
#
# print "hires  time = " . Time::PossiblyHiRes::getTime() . "\n";
# print "lowres time = " . time()    . "\n";
#


# end package
1;
