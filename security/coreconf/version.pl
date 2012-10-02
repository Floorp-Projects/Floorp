#!/usr/sbin/perl
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Compose lowercase alphabet
@alphabet = ( "a", "b", "c", "d", "e", "f", "g", "h",
              "i", "j", "k", "l", "m", "n", "o", "p",
              "q", "r", "s", "t", "u", "v", "w", "x",
              "y", "z" );

# Compute year
$year = (localtime)[5] + 1900;

# Compute month
$month = (localtime)[4] + 1;

# Compute day
$day = (localtime)[3];

# Compute base build number
$version = sprintf( "%d%02d%02d", $year, $month, $day );
$directory = sprintf( "%s\/%s\/%d%02d%02d", $ARGV[0], $ARGV[1], $year, $month, $day );

# Print out the name of the first version directory which does not exist
#if( ! -e  $directory )
#{
    print $version;
#}
#else
#{
#    # Loop through combinations
#    foreach $ch1 (@alphabet)
#    {
#	foreach $ch2 (@alphabet)
#	{
#	    $version = sprintf( "%d%02d%02d%s%s", $year, $month, $day, $ch1, $ch2 );
#	    $directory = sprintf( "%s\/%s\/%d%02d%02d%s%s", $ARGV[0], $ARGV[1], $year, $month, $day, $ch1, $ch2 );
#	    if( ! -e  $directory )
#	    {
#		print STDOUT $version;
#		exit;
#	    }
#	}
#    }
#}

