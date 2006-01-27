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
#  P4 change browser
#  Depot statistics
#
#################################################################

#######
# Parameters:
#
######

$| = 1 ;

#
# Get parameter(s)
#
my $FSPC = P4CGI::cgi()->param("FSPC") ;
$FSPC = "//..." unless defined $FSPC ;
&P4CGI::bail("Invalid file spec.") if ($FSPC =~ /[<>"&:;'`]/);
my @FSPC = split(/\s*\+?\s*(?=\/\/)/,$FSPC) ;
$FSPC = "<tt>".join("</tt> and <tt>",@FSPC)."</tt>" ;
my $FSPCcmd = "\"" . join("\" \"",@FSPC) . "\"" ;

###
### subroutine findTime
###  A (really) poor mans version of mktime(3). 
###  Parameters: year,month,day,hour,min
###  Returns: time_t value that corresponds to above result (almost)
sub findTime($$$$$)
{
    my ($iyear,$imon,$iday,$ihour,$imin) = @_ ;
    $iyear -= 1900 ;
    $imon-- ;
    my $time = time() ;
    my $delta = int($time/2)+1 ;
    my $lastsgn = -1 ;
    my $n = 300 ;
    while($delta > 10) {
	last if $n-- == 0 ;
	my $sgn = 1 ;
	my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($time) ;
	my $i = ((((((((($iyear * 12) + $imon) * 32) + $iday) * 24) + $ihour) * 60) + $imin) * 60) + 30 ;
	my $o = ((((((((($year * 12) + $mon) * 32) + $mday) * 24) + $hour) * 60) + $min) * 60) + $sec ;
       	last if $i == $o ;
	$sgn = -1 if $i < $o ;
	$time += ($sgn * $delta) ;
	$delta = int(($delta+1)/2) ;
	$lastsgn = $sgn ;
    }
    return $time ;
} ;

&P4CGI::SET_HELP_TARGET("DepotStats") ;

print &P4CGI::start_page("Depot Statistics for<br><tt>".
			 join("<br></tt>and<tt><br>\n",@FSPC) . "</tt>" ,
			 &P4CGI::ul_list(&P4CGI::ahref(-url => &P4CGI::cgi()->self_url . "#weekly",
						       "Weekly Submit Statistics"),
					 &P4CGI::ahref(-url => &P4CGI::cgi()->self_url . "#byuser",
						       "Submit Statistics by user")
					 )) ;

sub printStat($$) {
    my $prompt = shift @_ ;
    my $data   = shift @_ ;
    print 
	&P4CGI::table_row({-type => "th",
			   -align => "right",
			   -valign => "top",
			   -width => "50%",
			   -text => "$prompt:"},
			  {-type => "td",
			   -align => "left",
			   -width => "50%",
			   -text => $data}) ;
};
	
print 
    "<h2>Depot statistics</h2>",
    &P4CGI::start_table("") ;

{
    my @counters ;
    &P4CGI::p4call(\@counters,"counters") ;
#    printStat("P4 counters","") ;
    foreach (@counters) {
	s/(\S+) = /P4 $1 counter = / ;
	&printStat(split(" = ","$_")) ;	
    }
}
				       
				# Users
my @users ;
&P4CGI::p4call(\@users,"users") ;
printStat("Users",@users) ;
				# Clients
my @clients ;
&P4CGI::p4call(\@clients,"clients") ;
printStat("Clients",@clients) ;

				# Labels
my @labels ;
&P4CGI::p4call(\@labels,"labels") ;
printStat("Labels",@labels) ;

				# branches
my @branches ;
&P4CGI::p4call(\@branches,"branches") ;
printStat("Branches",@branches) ;

				# jobs
my @jobs ;
&P4CGI::p4call(\@jobs,"jobs") ;
printStat("Jobs",@jobs) ;

				       
print &P4CGI::end_table(),"<hr>" ;

				# Get changes
my @changes ;
&P4CGI::p4call(\@changes,"changes -s submitted $FSPCcmd") ;
				# Sort and remove duplicates
{
    my @ch = sort { $a =~ /Change (\d+)/ ; my $ac = $1 ;
		    $b =~ /Change (\d+)/ ; my $bc = $1 ;
		    $bc <=> $ac } @changes ;
    my $last="" ;
    @changes = grep {my $l = $last ;
		     $last = $_ ;
		     $_ ne $l } @ch ;
}
				## File list stats
print 
    "<h2>Statistics for \"$FSPC\"</h2>",
    &P4CGI::start_table("") ;

printStat("Submitted changes",scalar @changes) ;

				# Data about first submit
my $first = pop @changes ;
push @changes,$first ;
$first =~ s/Change (\d+).*/$1/ ;

my %data ;
my $firstTime = 0;
my $firstDate = "";
my $daysSinceFirstSubmit = 0 ;
&P4CGI::p4readform("change -o $first",\%data) ;

if(exists $data{"Date"}) {
    $firstDate = $data{"Date"} ;        
    if($data{"Date"} =~ /(\d+).(\d+).(\d+).(\d+).(\d+)/) {
	$firstTime = findTime($1,$2,$3,$4,$5) ; 
	my $seconds = time() - $firstTime ;
	$daysSinceFirstSubmit = int($seconds/(24*3600)) ;
    }
}

				# Last submit
my $last = shift @changes ;
unshift @changes,$last ;
$last =~ s/Change (\d+).*/$1/ ;

my $lastTime=0 ;
my $lastDate="" ;
my $daysSinceLastSubmit=0 ;

&P4CGI::p4readform("change -o $last",\%data) ;

if(exists $data{"Date"}) {
    $lastDate = $data{"Date"} ;        
    if($data{"Date"} =~ /(\d+).(\d+).(\d+).(\d+).(\d+)/) {
	$lastTime = findTime($1,$2,$3,$4,$5) ; 
	my $seconds = time() - $lastTime ;
	$daysSinceLastSubmit = int($seconds/(24*3600)) ;
    }
} ;

printStat("First submit","$first ($firstDate)") ;
printStat("Latest submit","$last ($lastDate)") ;
printStat("Days between first and latest submit",$daysSinceFirstSubmit-$daysSinceLastSubmit) ;
if(($daysSinceFirstSubmit-$daysSinceLastSubmit) > 0) {
    printStat("Average submits per day",
	      sprintf("%.2f",@changes/($daysSinceFirstSubmit-$daysSinceLastSubmit))) ;
};


				# Read and parse file list
my $files=0 ;
my $deletedFiles=0 ;
my %revlevels ;
my $maxrevlevel=0 ;
my $totrevs=0 ;
my $file ;
foreach $file (@FSPC) {
    local *F ;
    &P4CGI::p4call(*F,"files \"$file\"") ;
    while(<F>) {
	$files++ ;
	/\#(\d+) - (\S+)/ ;
	my ($r,$s) = ($1,$2) ;
	$deletedFiles++ if $s eq "delete" ;
	$totrevs += $r ;
	$maxrevlevel = $r if $r > $maxrevlevel ;
	$revlevels{$r} = 0 unless exists $revlevels{$r} ;
	$revlevels{$r}++ ;
    }
    close F ;
}

printStat("Current number of files",$files) ;
printStat("Deleted files",$deletedFiles) ;
printStat("Average revision level for files ",sprintf("%.2f",$totrevs/$files)) ;
printStat("Max revision level",$maxrevlevel) ;

print &P4CGI::end_table(),"<hr>" ;

				# File revision statistics
#  print 
#      "<a name=\"revstat\"><hr></a>",
#      &P4CGI::start_table("width=90%"),
#      &P4CGI::table_row(-type=>"th",
#  		      undef,
#  		      undef,
#  		      "File Revision Statistics"),
#      &P4CGI::table_row({-type=>"th",
#  		       -text => "Revision Level",
#  		       -width => "20%",
#  		       -align => "right"},
#  		      {-text => "No. of<br>files",
#  		       -type=>"th",
#  		       -width => "10%"},
#  		      {-text => "&nbsp;",
#  		       -bgcolor=>&P4CGI::BGCOLOR()}),
#      &P4CGI::end_table() ;
#
#my $max = 0 ;
#
#foreach (keys %revlevels) {
#    $max = $revlevels{$_} if $max < $revlevels{$_} ;
#} ;
#
#  my $rev=$maxrevlevel ;
#  while($rev > 0) {
#      my $n = 0 ;
#      $n = $revlevels{$rev} if exists $revlevels{$rev} ;
#      my $w = int((65.0 * $n)/$max) ;
#      if($w == 0) { $w = 1 ; } ;
#      print
#  	&P4CGI::start_table("colums=4 width=90% cellspacing=0"),
#  	&P4CGI::table_row({-text => "$rev",
#  			   -width => "20%",
#  			   -align => "right"},
#  			  {-text => $n==0?"-":"$n",
#  			   -align => "center",
#  			   -width => "10%"},
#  			  {-text => "&nbsp; ",
#  			   -bgcolor => $n!=0?"blue":&P4CGI::BGCOLOR(),
#  			   -width => "$w\%"},
#  			  {-text => "&nbsp;",
#  			   -bgcolor=>&P4CGI::BGCOLOR()}) ;
#      print &P4CGI::end_table() ;
#      $rev-- ;
#  }

my %dailySubStat ;
my %userSubStat ;
my $n ;


#my $time = time() ;
my $time = $lastTime ;
my $ONE_DAY=3600*24 ;
my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($time);
while($wday != 0) {
    $time -= $ONE_DAY ;
    ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($time);
}

sub getNextDate() {
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($time);
    $time -= $ONE_DAY * 7 ;
    my $day = sprintf("%4d/%02.2d/%02.2d",$year + 1900, $mon + 1, $mday) ;
    $dailySubStat{$day} = 0 ;
    return $day ;
}  ;
				# Read and parse change list

my $day = getNextDate() ;
my $max = 0 ;
while(@changes > 0) {
    $_ = shift @changes ;
    if(/Change \d+ on (\S+) by (\S+)\@/) {	
	my $d = $1 ;
	my $user = $2 ;
	while($d lt $day) {
	    $day = getNextDate() ;
	}
	$dailySubStat{$day}++ ;
	$max = $dailySubStat{$day} if $dailySubStat{$day} > $max ;
	$userSubStat{$user} = 0 unless exists  $userSubStat{$user} ;
	$userSubStat{$user}++ ;
    }
}

				# Weekly Submit Statistics
print "<a name=\"weekly\"></a><H2>Weekly Submit Rate for $FSPC</H2>",
    &P4CGI::start_table("width=90%"),
    &P4CGI::table_row({-type=>"th",
		       -text => "Week starting",
		       -width => "20%",
		       -align => "right"},
		      {-text => "submits",
		       -type=>"th",
		       -width => "10%"},
		      {-text => "&nbsp;",
		       -bgcolor=>&P4CGI::BGCOLOR()}),
    &P4CGI::end_table() ;


my $d ;
foreach $d (sort { $b cmp $a } keys %dailySubStat) {    
    print &P4CGI::start_table("colums=4 width=90% cellspacing=0") ;
    my $n = $dailySubStat{$d} ;
    my $w = int((65.0 * $n)/$max) ;
    if($w == 0) { $w = 1 ; } ;
    print &P4CGI::table_row({-text => "$d",
			     -width => "20%",
			     -align => "right"},
			    {-text => $n==0?"-":"$n",
			     -align => "center",
			     -width => "10%"},
			    {-text => "&nbsp; ",
			     -bgcolor => $n!=0?"blue":&P4CGI::BGCOLOR(),
			     -width => "$w\%"},
			    {-text => "&nbsp;",
			     -bgcolor=>&P4CGI::BGCOLOR()}) ;
    print &P4CGI::end_table() ;
}

				# Submits per user
print 
    "<a name=\"byuser\"><hr></a><h2>Submits by user in $FSPC</h2>",
    &P4CGI::start_table("width=90%"),
    &P4CGI::table_row({-type=>"th",
		       -text => "User",
		       -width => "20%",
		       -align => "right"},
		      {-text => "Submits",
		       -type=>"th",
		       -width => "10%"},
		      {-text => "&nbsp;",
		       -bgcolor=>&P4CGI::BGCOLOR()}),
    &P4CGI::end_table() ;

# Get users
my @listOfUsers = sort { uc($a) cmp uc ($b) } map { /^(\S+).*> \((.+)\) .*$/ ; $1 ; } @users ;
my %userCvt =  map { /^(\S+).*> \((.+)\) .*$/ ; ($1,$2) ; } @users ;


my $u ;
$max = 0 ;
foreach $u (sort {$userSubStat{$b} <=> $userSubStat{$a} ; } keys %userSubStat) {
    my $n = $userSubStat{$u} ;
    $max = $n if $max == 0 ;
    my $w = int((65.0 * $n)/$max) ;
    if($w == 0) { $w = 1 ; } ;
    if(exists $userCvt{$u}) {
	my $fullUser = $userCvt{$u} ;
	$u = &P4CGI::ahref(-url => "userView.cgi",
			   "USER=$u",
			   $fullUser) ;
    }
    else {
	$u = "<b>Old user:</b> $u"
    }
    print
	&P4CGI::start_table("colums=4 width=90% cellspacing=0"),
	&P4CGI::table_row({-text => "$u",
			   -width => "20%",
			   -align => "right"},
			  {-text => $n==0?"-":"$n",
			   -align => "center",
			   -width => "10%"},
			  {-text => "&nbsp; ",
			   -bgcolor => $n!=0?"blue":&P4CGI::BGCOLOR(),
			   -width => "$w\%"},
			  {-text => "&nbsp;",
			   -bgcolor=>&P4CGI::BGCOLOR()}) ;
    print &P4CGI::end_table() ;

}

print &P4CGI::end_page() ;        
    
#
# That's all folks
#









