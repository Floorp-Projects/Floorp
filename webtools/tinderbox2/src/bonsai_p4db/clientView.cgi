#!/usr/bin/perl -Tw
# -*- perl -*-

use lib '.';
use P4CGI ;
use strict ;
#
#################################################################
#  CONFIGURATION INFORMATION 
#  All config info should be in P4CGI.pm
#
#################################################################
#
#  View a p4 client
#
#################################################################

# Get parameter
my $client = P4CGI::cgi()->param("CLIENT") ;
$client = &P4CGI::extract_printable_chars($client);
unless(defined $client) {
    &P4CGI::bail("No client specified!") ;
} ;
&P4CGI::bail("Invalid client specified!") if ($client =~ /[<>"&:;'`]/);


# Get list of users and full names
my @users ;
@users=&P4CGI::run_cmd("users" );
my %users ;
map { s/^(\S+).* \((.*)\).*$/$1/ ; $users{$_}=$2 ; } @users ;

# Get client info
my %values ;
my @fields = &P4CGI::p4readform("client -o $client",\%values);

				# Fix owner field
if(exists $users{$values{"Owner"}}) {
    $values{"Owner"} = &P4CGI::ahref(-url => "userView.cgi" ,
				     "USER=$values{Owner}",
				     $values{"Owner"}) . " ($users{$values{Owner}})" ;  
}
else {
    $values{"Owner"} .= " <font color=red>No such user</font>" ;
} ;

				# Fix up description
{
    my $d =  &P4CGI::fixSpecChar($values{"Description"}) ;
    $d =~ s/\n/<br>/g ;
    $values{"Description"} = $d ;
}

				# Fix Root
if(exists $values{"Root"}) {
    $values{"Root"} = "<tt>$values{Root}</tt>" ;
} ;

				# Fix Options
if(exists $values{"Options"}) {
    $values{"Options"} = "<tt>$values{Options}</tt>" ;
} ;
				# Fix view
{
    my $view = &P4CGI::start_table("border=0 cellspacing=0 cellpadding=0") ;
    foreach (split("\n",$values{"View"})) {
	last if  /^\s*$/ ;
	my ($d,$c) = split(/\s+\/\//,$_) ;
	$view .= &P4CGI::table_row("<tt>$d</tt>","<tt>&nbsp;//$c</tt>") ;	    
    } ;	  
    $view .= &P4CGI::end_table() ;
    $values{"View"} = $view ;
}

$| = 1 ;

print "",
    &P4CGI::start_page("Client<br><tt>$client</tt>",
		       &P4CGI::ul_list("<b>user</b> -- view user info",
				       "<b>open file</b> -- view file log",
				       &P4CGI::ahref(-url => "changeList.cgi",
						     "CLIENT=$client",
						     "FSPC=//...",
						     "List changes by client") . 
				       " -- List changes made by client $client")) ;

				# Check that client exist
unless(exists $values{"Client"}) {
    &P4CGI::signalError("Client $client does not exist") ;
}

print 
    &P4CGI::start_table("") ;

my $f ; 
foreach $f (@fields) 
{
    print  &P4CGI::table_row({-align => "right",
			      -valign => "top",
			      -type  => "th",
			      -text  => $f},
			     $values{$f}) ;
} ;

my $openfiles ;
&P4CGI::p4call(*P4, "opened -a" );
while(<P4>) {
    chomp ;
    /^(.+\#\d+) - (\S+) .* by (\S+)\@(\S+)/ and do {
	my $f = $1 ;
	my $u = $3 ;
	my $r = "<b>$2</b>" ;
	my $c = $4 ;
	if($c eq $client) {
	    $f =~ /(.*)\#(\d+)/ ;
	    $f = &P4CGI::ahref(-url => "fileLogView.cgi",
			       "FSPC=$1",
			       "REV=$2",
			       "<tt>$f</tt>") ;
	    if(defined $openfiles) {
		$openfiles .= "<br>$f -&nbsp;$r" ;
	    } else {
		$openfiles = "$f -&nbsp;$r" ;	
	    } ;
	    if($u ne $values{"Owner"}) {
		$openfiles .= "&nbsp;by&nbsp;user&nbsp;$u" ;
	    }
	} ;
    } ;
} ;

if(defined $openfiles) {
    print  &P4CGI::table_row({-align  => "right",
			      -type   => "th",
			      -valign => "top",
			      -text   => "Open&nbsp;files:"},
			     "$openfiles") ;
} ;


print 
    &P4CGI::end_table(),
    &P4CGI::end_page() ;

#
# That's all folks
#




