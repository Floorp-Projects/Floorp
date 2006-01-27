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
#  List p4 clients
#
#################################################################

sub weeksago($$$ ) {
# Take Year, month and day of month as parameter and return the number
# of week that has passed since that date
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
				# The algorithm is not very robust, take current date and
				# remove one day at the time until the date match the requested
				# date. Can fail miserably for a number of combinations of
				# illegal input....
    while(1) {
	($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) =
	    localtime($_then);
	if(($y == $year) and ($m == $mon) and ($d == $mday)) {
	    return int(( $_now - $_then)/(3600*24*7)) ;
	}
	$_then -= 3600*24 ;
    } ;	
}

my $user = P4CGI::cgi()->param("USER") ;
$user = &P4CGI::extract_user($user) ;

my $mode = P4CGI::cgi()->param("MODE") ;
$mode = "Brief" unless (defined $mode) and ($mode eq "Complete") ;
$mode = "Complete" if ($user ne '');

# Get clients
my @tmp ;
@tmp=&P4CGI::run_cmd( "clients" );
my @clients ;
my %clientInfo ;
my $c ;
foreach $c (@tmp) {
    if($c =~ /^Client\s+(\S+)\s+(\S+)\s+root\s+(.*)\s+'(.*)'/)
    {
	my ($client,$updated,$root,$desc) = ($1,$2,$3,$4) ;
	push @clients,$client ;
	my %tmp = ("Update" => $updated,
		   "Root" => $root,
		   "Description" => $desc) ;
	$clientInfo{$client} = \%tmp ;
    }
} ;

my $clients = @clients ;
my $warnings = 0 ;

my $title = "P4 clients" ;
if(defined $user) {
    $title .= "<br>for user $user" ;
}

$| = 1 ;

my @legend = ("<b>client</b> -- see more info",
	      "<b>owner</b> -- see user info") ;

my $lastaccess = undef ;
my $owner = undef ;
if($mode eq "Brief") {
    push @legend,&P4CGI::ahref("MODE=Complete",
			       "<B>Show owner and access info</B>") ;
}
else {
    $lastaccess = "Last access" ;
    $owner = "Owner/view user" ;
} ;
if(defined $user) {
    push @legend,&P4CGI::ahref("<B>Show all clients</B>") ;    
}



print "",
    &P4CGI::start_page($title,
		       &P4CGI::ul_list(@legend)) ;
unless(defined $user) {
    print  "<B>", $clients," clients</B><br> " ;
}
print "",
    &P4CGI::start_table(" cellpadding=1"),
    &P4CGI::table_header("Client/view client",$owner,"Description",
			 "Updated     ",$lastaccess);


# Get users
my @users ;
@users=&P4CGI::run_cmd( "users" );
my %users ;
map { s/^(\S+).*$/$1/ ; $users{$_}="" ; } @users ;

if($mode ne "Brief") {
    my $client ;
    foreach $client (sort { uc($a) cmp uc($b) } @clients)
    {
	my %values ;
  print "user $user mode $mode";
	my @fields = &P4CGI::p4readform("client -o $client",\%values) ;
	my $warning = "" ;
	if(exists $values{"Date"}) {
	    $values{"Update"} = $values{"Date"} ;
	    $values{"Access"} = "---" ;
	    delete $values{"Date"} ;
	}
	else {
	    if($values{"Access"} =~ /(\d\d\d\d)\/(\d\d)\/(\d\d)/) {
		my $weeksOld = weeksago($1,$2,$3) ;
		if($weeksOld > 10) {
		    if($warning ne "") { $warning .= "<br>\n" ; } ;
		    $warning .= "Not used for $weeksOld weeks!" ;		    
		}
	    }
	}
	if(exists $values{"Owner"}) {
	    $owner = $values{"Owner"} ;
	    $values{"OwnerName"} = $owner ;
	    if(exists $users{$owner}) {
		$values{"Owner"} = &P4CGI::ahref(-url => "userView.cgi" ,
						 "USER=$owner",
						 $owner),
	    }
	    else {		
		if($warning ne "") { $warning .= "<br>\n" ; } ;
		$warning .= "Owner does not exist!" ;
	    }	    
	} ;
	if(exists $values{"Description"}) {
	    $values{"Description"} = P4CGI::fixSpecChar($values{"Description"}) ;
	    $values{"Description"} =~ s/\n/<br>\n/sg ;
	}
	unless((defined $user) and ( uc($user) ne uc($owner))) {
	    $values{"Warnings"} = $warning ;    
	    $clientInfo{$client} = { %{$clientInfo{$client}},%values} ;
	    if($warning ne "") { $warnings++ ; } ;
	}
    } ;
}

my $client ;
foreach $client (sort { uc($a) cmp uc($b) } @clients)
{        
    my %info = %{$clientInfo{$client}} ;

    $info{"Warnings"} = "" unless defined $info{"Warnings"} ;

    if((!defined $user) or (uc($user) eq uc($info{"OwnerName"}))) {	
	print &P4CGI::table_row(-valign=>"top",
				&P4CGI::ahref(-url => "clientView.cgi",
					      "CLIENT=$client",
					      $client),
				$info{"Owner"},
				{
				    -text => "<tt>" . $info{"Description"} . "</tt>",
				},
				$info{"Update"},
				$info{"Access"},
				"<font color=red><b>$info{Warnings}</b></font>") ;
    }
}
print &P4CGI::end_table() ;
    
if($warnings > 0) {
    my $s = "" ;
    $s = "s" if $warnings != 1 ;
    $warnings = "<font color=red>($warnings warning$s)</font>" ;
}
else {
    $warnings = "" ;    
}

print
    " $warnings<br>",
    &P4CGI::end_page() ;

#
# That's all folks
#
