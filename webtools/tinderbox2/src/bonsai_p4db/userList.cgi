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
#  P4 list all users
#  List p4 users
#
#################################################################


my $GROUPSONLY = P4CGI::cgi()->param("GROUPSONLY") ;
$GROUPSONLY = "Y" if defined $GROUPSONLY;

sub weeksago($$$ ) {
    my ($y,$m,$d) = @_ ;
    $y -= 1900 ;
    $m-- ;
    my $_now = time() ;
    my $_then = $_now ;
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) =
	localtime($_then);
    if(($y > $year) or
       (($y == $year) and ($m > $mon)) or
       (($y == $year) and ($m == $mon) and ($d > $mday))) {
	return 0 ;
    }
    while(1) {
	($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) =
	    localtime($_then);
	if(($y == $year) and ($m == $mon) and ($d == $mday)) {
	    return int(( $_now - $_then)/(3600*24*7)) ;
	}
	$_then -= 3600*24 ;
    } ;	
}

&P4CGI::SET_HELP_TARGET("userList") ;

# Get users
my @users ;
@users=&P4CGI::run_cmd("users" );

# Get groups
my @groups ;
@users=&P4CGI::run_cmd("groups" );

my $emailall ;
my $usertable = "" ;
unless(defined $GROUPSONLY) {
    $usertable .=  join("\n",("<B>",scalar(@users)," users:<br></B>",
			      &P4CGI::start_table("cellpadding=1"),
			      &P4CGI::table_header("User/view user",
						   "Name",
						   "e-mail address/send email",
						   "Last access"))) ;
    
    my $userinfo ;
    foreach $userinfo (sort { uc($a) cmp uc($b) } @users)
    {
	$userinfo =~ /(\w+)\s+\<(.*)\>\s+\((.*)\) accessed (\S+)/ and do {
	    my ($user,$email,$name,$lastaccess) = ($1,$2,$3,$4) ;
	    $user = &P4CGI::ahref(-url => "userView.cgi",
				  "USER=$user",
				  $user) ;
	    $email =~ /\w+\@\w+/ and do {
		if(defined $emailall) {
		    $emailall .= ",$email" ;
		} else {
		    $emailall = "mailto:$email" ;
		} ;
		$email = &P4CGI::ahref(-url => "mailto:$email",
				       $email) ;
	    } ;
	    my $weeksOld = "" ;
	    if($lastaccess =~ /(\d\d\d\d)\/(\d\d)\/(\d\d)/) {
		$weeksOld = weeksago($1,$2,$3) ;
		if($weeksOld > 10) {
		    $weeksOld = "<b>Not used for $weeksOld weeks!</b>" ;
		}
		else {
		    $weeksOld = "" ;
		}
	    }
	    $usertable .= &P4CGI::table_row($user,
					    $name,
					    $email,
					    $lastaccess,
					    $weeksOld) ;	
	}
    }
    $usertable .= &P4CGI::end_table() ;
} ;

if(@groups > 0) {
    my $g = @groups == 1?"group":"groups" ;
    my $n = @groups ;
    $usertable .=  "<B>$n $g</B><br>" ;
    $usertable .=  &P4CGI::start_table("cellpadding=1") ;
    $usertable .=  &P4CGI::table_header("Group/view group");
    foreach (@groups) {
	$usertable .=  &P4CGI::table_row(&P4CGI::ahref(-url => "groupView.cgi",
						       "GROUP=$_",
						       $_)) ;
    }
    $usertable .= &P4CGI::end_table() ;
}

my @legend ;

unless(defined $GROUPSONLY) {
    push @legend ,("<b>user</b> -- see more info",
		   "<b>e-mail address</b> -- e-mail user",
		   &P4CGI::ahref(-url => $emailall,
				 "<b>Email all users</b>")) ;
} ;

push @legend,"<b>group</b> -- details about group" if @groups > 0 ;
unless(defined $GROUPSONLY) {
    push @legend, &P4CGI::ahref("GROUPSONLY=Y",
				"<b>Groups only</b>") ;
}

print "",
    &P4CGI::start_page(defined $GROUPSONLY ? "P4 Groups":@groups > 0?"P4 Users and Groups":"P4 Users",
		       &P4CGI::ul_list(@legend)),
    $usertable,
    &P4CGI::end_page() ;

#
# That's all folks
#
