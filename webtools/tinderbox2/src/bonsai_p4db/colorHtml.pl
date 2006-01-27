#!/usr/bin/perl -w
#
# This script is eval'ed by the fileViewer.cgi script.
#
# Input is a scalar, $FILE, that contains the file text
#
#
#################################################################
#
#  Add colour to html
#
#################################################################

my $bracketColor="blue" ;
my $firstWordColor="blue" ;
my $restWordsColor="green" ;

my $leftbr = "<font color=$bracketColor>&lt;</font>" ;
my $rightbr = "<font color=$bracketColor>&gt;</font>" ;


$FILE =~ s/&lt;(\/{0,1}\w+)(.*?)&gt;/$leftbr<font color=$firstWordColor>$1<\/font><font color=$restWordsColor>$2<\/font>$rightbr/g ;
$FILE =~ s/&lt;(!.*?)&gt;/$leftbr<font color=$bracketColor><b>$1<\/b><\/font>$rightbr/ ;

#
# End
#
