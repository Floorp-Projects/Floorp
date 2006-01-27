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
#  P4 view a group
#  
#
#################################################################

$| = 1 ; # turn off output buffering

# Get parameter
my $group = P4CGI::cgi()->param("GROUP") ;
unless(defined $group) {
    &P4CGI::bail("No group specified!") ;
} ;

&P4CGI::bail("Invalid group.") if ($group =~ /[<>"&:;'`]/);

				# Get real user names...
my %userCvt ;
{
    my @users ;
    &P4CGI::p4call(\@users, "users" );
    %userCvt =  map { /^(\S+).*> \((.+)\) .*$/ ; ($1,$2) ; } @users ;
}

my %groups ;
{
    my @groups ;
    &P4CGI::p4call(\@groups, "groups" );
    %groups = map { ($_,1) ; } @groups ;
}

# Get user info
my %values ;
my @fields = &P4CGI::p4readform("group -o $group",\%values);

print "",
    &P4CGI::start_page("Group $group",
		       &P4CGI::ul_list("<b>user</b> -- view user",
				       &P4CGI::ahref(-url => "changeList.cgi",
						     "GROUP=$group",
						     "FSPC=//...",
						     "List changes by group") . 
				       " -- List changes made by group $group")) ;

unless(exists $groups{$group}) {
    &P4CGI::signalError("No such group \"$group\"") ;
}

print 
    &P4CGI::start_table("") ;

my @emailUsers ;

if(exists $values{"Users"}) {
    my @users ;
    foreach (split( /\s+/,$values{"Users"})) {
	my $fullname ;
	if(exists $userCvt{$_}) {
	    $fullname = "($userCvt{$_})" ;
	    push @emailUsers,$_ ;
	}
	else {
	    $fullname = "(<font color=red>No such user</font>)" ;
	} ;
	push @users,  &P4CGI::ahref(-url => "userView.cgi",
				    "USER=$_",
				    "$_ $fullname") ;
    } ;
    $values{"Users"} = join("<br>\n",@users) ;
} ;


if(exists $values{"Subgroups"}) {
    my @subgroups ;
    foreach (split( /\s+/,$values{"Subgroups"})) {
	my $sg ;
	if(exists $groups{$_}) {
	    push @subgroups, &P4CGI::ahref(-url => "groupView.cgi", # 
					   "GROUP=$_",
					   $_) ;
	}
	else {
	    push @subgroups, "$_ (<font color=red>No such group</font>)" ;
	} ;
    } ;
    $values{"Subgroups"} = join("<br>\n",@subgroups) ;
} ;

my $f ;
foreach $f (@fields) {
    print &P4CGI::table_row({-align => "right",
			     -valign => "top",
			     -type  => "th",
			     -text  => "$f"},
			    $values{$f}) ;
} ;


print &P4CGI::end_table() ;

if(@emailUsers > 0) {
    my @email ;
    foreach (@emailUsers) {
	my %data ;
	&P4CGI::p4readform("user -o $_",\%data) ;
	if(exists $data{"Email"}) {
	    push @email,$data{"Email"} ;
	}	
    }
    my $email = join(",",@email) ;
    print "<br><a href=\"mailto:$email?Subject=To members in group $group\">Email all group members</a><br>" ;
}

print &P4CGI::end_page() ;

#
# That's all folks
#
