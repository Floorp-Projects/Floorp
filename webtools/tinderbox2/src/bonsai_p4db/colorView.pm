#!/usr/bin/perl -w
# -*- perl -*-
package colorView ;
use strict ;
#
#
#################################################################
# Configuration file for color view of files
#################################################################
#
#
				# This module is designed to add colors to
				# file view.
				# The idea is to apply a filter (in this case
				# eval a perl script) that adds color to
				# the file. The filter is selected using
				# one of two methods:
				# 1. Check file name (typicall extension)
				# 2. Check first line in file

my %fileNameRegexp ;
# Store filter name by regexp for file name 

my %firstLineRegexp ;
# Store filter name by regexp for first line in file to color

				# Initialize variables
BEGIN() 
{
    %fileNameRegexp = ("\\\.html\$" => "colorHtml.pl" ,
		       "\\\.htm\$"  => "colorHtml.pl" ,
		       "\\\.pl\$"   => "colorPerl.pl",
		       "\\\.c\$"   => "colorC.pl",
		       "\\\.C\$"   => "colorC.pl",
		       "\\\.cxx\$"   => "colorC.pl",
		       "\\\.cpp\$"   => "colorC.pl",
		       "\\\.h\$"   => "colorC.pl",
		       "\\\.H\$"   => "colorC.pl",
		       "\\\.hxx\$"   => "colorC.pl",
		       "\\\.hpp\$"   => "colorC.pl",		       
		       ) ;
    %firstLineRegexp = ("perl"      => "colorPerl.pl") ;
} ;

				# Subroutine to call
sub color($,\$) {
    my $filename = shift @_ ;
    my $textref = shift @_ ;
    my $t ;
    my $FILE = $$textref ;
    foreach $t (keys %fileNameRegexp) {
	if($filename =~ /$t/) {
	    eval `cat $fileNameRegexp{$t}` ;
	    $$textref = $FILE unless $@ ;
	    return ;
	}
    } ;
    $FILE =~ /^(.*?)\n/ ;
    my $firstLine = $1 ;
    foreach $t (keys %firstLineRegexp) {
	if($firstLine =~ /$t/) {
	    eval `cat $firstLineRegexp{$t}` ;
	    $$textref = $FILE unless $@ ;
	    return ;
	}
    }
} ;

1;
		       


