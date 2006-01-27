#!/usr/bin/perl -w
# -*- perl -*-
use P4CGI ;
use strict ;
#
#################################################################
#  CONFIGURATION INFORMATION 
#  All config info should be in P4CGI.pm
#
#################################################################
#
#  P4 file download
#  Download a file
#
#################################################################

# Get file spec argument
my $file = P4CGI::cgi()->param("FSPC") ;
&P4CGI::bail("No file specified") unless defined $file ;
&P4CGI::bail("Invalid file.") if ($file =~ /[<>"&:;'`]/);

my $filename = $file ;
$filename =~ s/.*\/// ;

my $revision = P4CGI::cgi()->param("REV") ;
&P4CGI::bail("No revision specified") unless defined $revision ;
&P4CGI::bail("Invalid revision specified") unless $revision =~ /^\d*$/;

local *P4 ;

&P4CGI::p4call( *P4, "print -q \"$file#$revision\"" );
$file = join('',<P4>) ;
close P4 ;

my $len = length($file) ;

print 
    "Content-type: application/octet-stream\n",
    "Content-Disposition: attachment;filename=$filename;size=$len\n",
    "Content-Description: Download file:$filename Rev:$revision\n",
    "\n" ;
print $file ;

#
# That's all folks
#
