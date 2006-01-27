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
#  P4 list user
#  List a p4 user
#
#################################################################

# Get parameter
my $user = P4CGI::cgi()->param("USER") ;
$user =  P4CGI::extract_user($user);
# users may be specified with a client name, 
# but do not look them up that way.
$user =~ s/\@.*$//;

unless(defined $user) {
    &P4CGI::bail("No user specified!") ;
} ;


# List all users
my @userData = &P4CGI::run_cmd("users" );
my %userData = map { /^(\S+)/ ; ($1,1) ; } @userData ;

# Get user info
my %values ;
my @fields ;
if(exists $userData{$user}) {
    @fields = &P4CGI::p4readform("user -o $user",\%values);
}

				# Fix email
if(exists $values{"Email"}) {
    my $em = &P4CGI::fixSpecChar($values{"Email"}) ;
    $values{"Email"}=&P4CGI::ahref(-url => "mailto:$values{Email}",$em) ;
}
				# Fix fullname
if(exists $values{"FullName"}) {
    $values{"FullName"} = &P4CGI::fixSpecChar( $values{"FullName"}) ;
}
				# Fix job view
if(exists $values{"JobView"}) {
    my $v = $values{"JobView"} ;
    $values{"JobView"} = &P4CGI::ahref(-url => "jobList.cgi",
				       "JOBVIEW=$v",
				       "LIST=Y",
				       $v) ;
}
				# Fix group view
{
    my @groups ;
    @groups=&P4CGI::run_cmd("groups", $user) ;
    if(@groups > 0) {
	my $p = "In group" ;
	if(@groups > 1) { $p .="s" ; } ;
	push @fields,$p ;
	$values{$p} = join(",", map { &P4CGI::ahref(-url => "groupView.cgi",
						    "GROUP=$_",
						    $_) ; } @groups) ;	
    }
}


print "",
    &P4CGI::start_page("User $user",
		       &P4CGI::ul_list("<b>e-mail address</b> -- e-mail user",
				       "<b>JobView</b> -- list jobs for this view",
				       &P4CGI::ahref(-url => "clientList.cgi",
						     "USER=$user",
						     "List clients") . 
				       " -- List clients owned by user $user",
				       &P4CGI::ahref(-url => "changeList.cgi",
						     "USER=$user",
						     "FSPC=//...",
						     "List changes by user") . 
				       " -- List changes made by user $user",
				       &P4CGI::ahref(-url => "userList.cgi",
						     "List all users") . 
				       " -- List all users and groups",
				       )) ;

unless(exists $userData{$user}) {
    &P4CGI::signalError("User \"$user\" does not exist. ") ;
}


print 
    &P4CGI::start_table("") ;

my $f ;
foreach $f (@fields) {
    print &P4CGI::table_row({-align => "right",
			     -type  => "th",
			     -text  => "$f"},
			    $values{$f}) ;
} ;

my $openfiles ;
&P4CGI::p4call(*P4, "opened -a" );
my $line=0 ;
while(<P4>) {
    chomp ;
    / by $user\@/ and do {
	$line++ ;
	/^(.*\#\d+) - (\S+) .* by \w+\@(\S+)/ or do { &P4CGI::ERROR("Unable to parse line $line ($_)") ;
						      next ; } ;
	my $file = $1 ;
	my $reason = $2 ;	
	my $client = $3 ;
	$client = &P4CGI::ahref(-url => "clientView.cgi",
				"CLIENT=$client",
				"<tt>$client</tt>") ; 
	$file =~ /(.*)\#(\d+)/ ;
	if($reason ne "add") {
	    $file = &P4CGI::ahref(-url => "fileLogView.cgi",
				  "FSPC=$1",
				  "REV=$2",
				  "$file")  ;
	}
	if(defined $openfiles) {
	    $openfiles .= "<br><tt>$file</tt> -&nbsp;<b>$reason</b>&nbsp;by&nbsp;client&nbsp;$client" ;
	} else {
	    $openfiles = "<tt>$file</tt> -&nbsp;<b>$reason</b>&nbsp;by&nbsp;client&nbsp;$client" ;	
	} ;
    } ;
} ;
if(defined $openfiles) {
    print  &P4CGI::table_row({-align  => "right",
			      -type   => "th",
			      -valign => "top",
			      -text   => "Open&nbsp;files"},
			     "$openfiles") ;
} ;


print 
    &P4CGI::end_table(),
    &P4CGI::end_page() ;

#
# That's all folks
#
