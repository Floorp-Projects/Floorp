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
#  P4 special file viewer for HTML, JPEG and GIF files
#  View a "special" file
#
#################################################################

use viewConfig ;

$| = 1 ; # Turn output buffering off

# Get type arg
my $type = P4CGI::cgi()->param("TYPE") ;
&P4CGI::bail("No file type specified") unless defined $type ;
&P4CGI::bail("Invalid file type.") if ($type =~ /[<>"&:;'`]/);

# Get file spec argument
my $file = P4CGI::cgi()->param("FSPC") ;
&P4CGI::bail("No file specified") unless defined $file ;
&P4CGI::bail("Invalid file.") if ($file =~ /[<>"&:;'`]/);

my $revision = P4CGI::cgi()->param("REV") ;
$revision = "#$revision" if defined $revision ;
$revision="" unless defined $revision ;
&P4CGI::bail("Invalid revision.") unless ($revision =~ /^#?\d*$/);

my ($url,$desc,$content,$about) = @{$viewConfig::TypeData{$type}} ;
&P4CGI::bail("Undefined type code") unless defined $url ;

my $filename=$file ;
$filename =~ s/^.*\///;

print
    "Content-Type: $content\n",
    "Content-Disposition: filename=$filename\n",
    "\n" ;

local *P4 ;
&P4CGI::p4call( *P4, "print -q \"$file$revision\"" );
while(<P4>) {
    print ;
} ;       

#
# That's all folks
#
