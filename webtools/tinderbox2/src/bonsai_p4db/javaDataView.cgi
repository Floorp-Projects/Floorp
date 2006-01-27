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
#  Java depot browser cgi
#
#################################################################


				# * Get path from argument

my $cmd = P4CGI::cgi()->param("CMD") ;
&P4CGI::bail("Invalid command.") unless ($cmd =~ /^\w*$/);

my $err2null = &P4CGI::REDIRECT_ERROR_TO_NULL_DEVICE() ;

local *P4 ;

print
    "Content-type: text/plain\n" .
    "Pragma: no-cache\n" .
    "\n\n" ;

if($cmd eq "DIRSCAN") {
    my $fspc = P4CGI::cgi()->param("FSPC") ;
    my @dirs ;
    &P4CGI::p4call(\@dirs,"dirs -D \"$fspc\" $err2null") ;
    foreach (@dirs) {
	s/^.*\/// ;
	print "D \"$_\"\n" ;
    } ;
    my @files ;
    &P4CGI::p4call(\@files,"files \"$fspc\" $err2null") ;
    foreach (@files) {
	s/^.*\/(.*)\#(\d+) - (\w\w).*$/"$1" $2 $3/;
	print "F $_\n" ;
    } ;

}
if($cmd eq "FILES") {
    my $dir = P4CGI::cgi()->param("FSPC") ;
    &P4CGI::p4call(*P4,"files \"$dir\" $err2null") ;
    while(<P4>) {
	chomp ;
	s/^.*\/(.*)\#(\d+) - (\w\w).*$/"$1" $2 $3/;
	print "$_\n" ;
    } ;
    close *P4 ;
} ;

#
# That's all folks
#
