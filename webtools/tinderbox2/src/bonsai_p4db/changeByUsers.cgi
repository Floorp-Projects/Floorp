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
#  List changes by user and group
#
#################################################################


my $FSPC = P4CGI::cgi()->param("FSPC") || "//..." ;
$FSPC = P4CGI::extract_filename_chars($FSPC);

my $COMPLETE= &P4CGI::cgi()->param("COMPLETE") ;
if (defined($COMPLETE)) {
     $COMPLETE ="Yes";
}


my @legend ;

unless(defined $COMPLETE) {
    push @legend,&P4CGI::ahref("COMPLETE=Yes",
			       "FSPC=$FSPC",
			       "Include old users in list") ;
} ;


my $legend = "" ;
$legend = &P4CGI::ul_list(@legend) if @legend > 0 ;

print "",   &P4CGI::start_page("View changes by<br>User(s) and Group(s)",$legend) ;





# Get users
my @users ;
@users=&P4CGI::run_cmd("users");
my @listOfUsers = sort { uc($a) cmp uc ($b) } map { /^(\S+).*> \((.+)\) .*$/ ; $1 ; } @users ;
my %userCvt =  map { /^(\S+).*> \((.+)\) .*$/ ; ($1,$2) ; } @users ;

if(defined $COMPLETE) {
    my %allUsers ;
    local *P ;
    &P4CGI::p4call(*P, "changes $FSPC") ;
    while(<P>) {
	/.*by (\S+)@/ ;
	if(exists $allUsers{$1}) { $allUsers{$1} += 1 ; }
	else                     { $allUsers{$1} = 1 ; }
    } ;
    foreach (keys %allUsers) {
	if(!exists $userCvt{$_}) {
	    $userCvt{$_} = "Old user: $_ ($allUsers{$_} changes)" ;
	    push @listOfUsers,$_ ;
	} else {
	    $userCvt{$_} .= " ($allUsers{$_} changes)" ;
	}
    } ;
}


# Get groups
my @listOfgroups ;
@listOfgroups=&P4CGI::run_cmd("groups");


print "",
    &P4CGI::start_table("bgcolor=".&P4CGI::HDRFTR_BGCOLOR()." align=center cellpadding=0 cellspacing=2"),
    "<tr><td>\n" ;


sub prSelection($$$$ )
{
    my $cgitarget  = shift @_ ;
    my $desc       = shift @_ ;
    my $fields     = shift @_ ;
    my $helpTarget = shift @_ ;
    
    print "", &P4CGI::table_row(-valign=>"center",
				{-align=>"center",
				 -text => 
				     join("\n",
					  &P4CGI::cgi()->startform(-action => $cgitarget,
								   -method => "GET"),
					  "<font size=+1>$desc</font>")},
				{-align=>"left",
				 -valign=>"top",
				 -text => $fields},
				{-align=>"left",
				 -text => " "},
				{-align=>"left",
				 -valign=>"bottom",
				 -width=>"1",
				 -text =>  &P4CGI::cgi()->submit(-name  => "ignore",
								 -value => "GO!")
				 },
				{ -valign=>"bottom",
				  -text => &P4CGI::cgi()->endform()
				  },
				) ;
} ;

print "",  &P4CGI::start_table("width=100% cellspacing=4") ;

my $ulistSize = @listOfUsers ;
$ulistSize= 15 if $ulistSize > 15 ;

my $glistSize = @listOfgroups ;
$glistSize= 15 if $glistSize > 15 ;

prSelection("changeList.cgi",
	    "Select users and groups",
	    join("\n",
		 &P4CGI::start_table(),
		 &P4CGI::table_row(
				   "File spec:",
				   "<font face=fixed>" .
				   &P4CGI::cgi()->textfield(-name      => "FSPC",
							    -default   => $FSPC,
							    -size      => 50,
							    -maxlength => 256) .
				   "</font>"),
		 &P4CGI::table_row(-valign=>"top",
				   "User(s):",
				   "<font face=fixed>" .
				   &P4CGI::cgi()->scrolling_list(-name      => "USERS",
								 -values    => \@listOfUsers,
								 -size      => $ulistSize,
								 -multiple  => 'true',
								 -labels    => \%userCvt) .
				   "</font>"),
		 &P4CGI::table_row(-valign=>"top",
				   "Group(s):<font face=fixed>",
				   "<font face=fixed>" .
				   &P4CGI::cgi()->scrolling_list(-name      => "GROUPS",
								 -values    => \@listOfgroups,
								 -size      => $glistSize,
								 -multiple  => 'true') .
				   "</font>"),
		  &P4CGI::end_table()),
	    "user_and_group") ;

print &P4CGI::end_table() ;
		

print  "</tr></td>",&P4CGI::end_table() ;

print    
    &P4CGI::end_page() ;

#
# That's all folks
#




