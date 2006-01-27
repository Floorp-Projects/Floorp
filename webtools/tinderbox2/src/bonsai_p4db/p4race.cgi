#!/usr/bin/perl -w
# -*- perl -*-
use P4CGI ;
use strict ;
#################################################################
#
#
#                         The great
#
#                  P 4   S U B M I T   R A C E
#
#                          contest
#
#
#
# This is unpublished and unofficial and unsupported and untrusted
# and unbelivable non-proprietary source code of MYDATA automation AB
# Do whatever you want with it. We don't care.
#
# Send your comments, flames, fan email, threats, email bombs, spam etc to:
# fredric@mydata.se
#
# Troubleshooting guide:
# Apart from the usual problems with cgi's you might get in touble
# with the P4 protection system. I think you must have at least "list"
# access to the depot, but I have not tested this so.....
#
# Other than that: You're on your own buddy! 
#
#################################################################

#
my $DEFAULT_LENGTH = 499                 ; # Set default number of changes
#

my $changes = $DEFAULT_LENGTH ;

my %subsByUsr;
# Contains number of submits by user

my %userpoints;
# Contains "points" per user (actually, used to compute the 
# "mean position" of all the submits found by user). This is used to
# evaluate users "speed".

my %ptsInLast10;
# Contains number of submits in the oldest ten percent. A little "extra"
# feature to make comments more interesting.

#
# Find out if mozilla (a plot to make the user feel he is always wrong)
#
my $browser = $ENV{"HTTP_USER_AGENT"} ;
my $bestViewed = "Netscape Navigator <!-- not $browser -->";
my $blink="B" ;
if(($browser =~ /mozilla/i) and not ($browser =~ /msie/i)) {
    $bestViewed = "Microsoft Explorer <!-- not $browser -->" ;
    $blink="BLINK" ;
}

my $replay = &P4CGI::cgi()->param("REPLAY") ;
$replay = undef unless defined $replay and $replay =~ /^\d+$/ ;
$replay = 0 unless defined $replay ;

#
#  Read p4 repository
#
local *CHANGES ;

my $skip = $replay ;
my $ch = $changes + $replay ;

&P4CGI::p4call(*CHANGES,"changes -m $ch -s submitted") ;

my $tenPercent = $changes/10 ;
my $pos = 0 ;
my $lastUsr ;
my $firstUsr ;
my $leadChange ;
my $tmpch = $changes ;
while (<CHANGES>) {
    unless(defined $leadChange) {
	/^Change (\d+).*/ ;
	$leadChange = $1 ;
    };
    next if $skip-- > 0 ;
    $tmpch-- || do { last ; } ;
    /(\w+)@/ || do { next ; } ;
    $pos++ ;
    $lastUsr = $1 ;
    $firstUsr = $1 unless defined $firstUsr ;
    if(!$subsByUsr{$1}) { 
	$subsByUsr{$1} = 1 ; 
	$userpoints{$1} = $pos ;
    } 
    else {
	$userpoints{$1} += $pos ;
	$subsByUsr{$1}++ ;
    }
    if($tmpch < $tenPercent) {
	if(defined $ptsInLast10{$1}) { $ptsInLast10{$1} += 1 ; } 
	else                         { $ptsInLast10{$1} = 1 ; } ;
    }
}
my $total = $pos ;
close CHANGES;

my %userToName ;
local *F ;
&P4CGI::p4call(*F,"users") ;
while(<F>) {    
    /^(\w+).*>\s+\((.*)\)\s+acc/ || do { next ; } ;    
    $userToName{$1}=$2 ;
};
close(F) ;

##
## Write html code
##

#
# Get date and time
#
my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) =
    localtime(time());
my $date = sprintf("%d/%d-%4.4d %2.2d:%2.2d",
		   $mday,$mon+1,$year+1900,
		   $hour,$min) ;

my $replayUrl = "" ;
if($replay > 0) {
    my $r = $replay-1 ;
    &P4CGI::EXTRAHEADER(-Refresh => "4; URL=p4race.cgi?REPLAY=$r") ;
    $replayUrl = "<blink><font align=center color=red size=+1>Replay offset $replay</font></blink><br>" . &P4CGI::ahref(-url=>"p4race.cgi",
													   "Abort Replay") 
} else {
    &P4CGI::EXTRAHEADER(-Refresh => "30; URL=p4race.cgi") ;
    $replayUrl = &P4CGI::ahref("REPLAY=30",
			       "Replay the last 30 submits") ;

}

&P4CGI::SET_HELP_TARGET("P4Race") ;

print "",
    &P4CGI::start_page("P4 submit race <br><small>$date</small>",
		       "Distance: $changes changes<br>$replayUrl") ;


#
# Compute a background color
#
srand($leadChange) ;
my $bc=(((((rand(0x10)*5)+0xB0) & 0xf0)*0x10000)+
	((((rand(0x10)*5)+0xB0) & 0xf0)*0x100)  +
	(( (rand(0x10)*5)+0xB0) & 0xf0)) ;
my $lcolor=sprintf("%6.6X",$bc & 0xf0f0f0) ;

#
# Start building table
#
my $posInRace = 0;
my $prevUserPoints = 0;
my $usersAtSamePos = 1;
my $usr ;
my $table="first" ;
print "<P>",
    &P4CGI::start_table("BORDER=10 ALIGN=CENTER BGCOLOR=$lcolor"),
    &P4CGI::table_row(-type=>"th",
		      "Position",
		      "User",
		      "# of submits",
		      "Comment") ;

foreach $usr (sort { $subsByUsr{$b} <=> $subsByUsr{$a} } (keys %subsByUsr)){
    #
    # Compute weighted mean position for users submits
    #
    my $meanPos = 100*(1-($userpoints{$usr}/($subsByUsr{$usr}*$total))) ;
    
    #
    # Set a status message depending on mean pos
    #
    my $status ;
    $status = "A LOSER" ;
    $status = "Losing position fast"  if $meanPos > 10 ;
    $status = "Losing"                if $meanPos > 25 ;
    $status = "Losing slowly"         if $meanPos > 33 ;
    $status = "Almost keeping pace"   if $meanPos > 43 ;
    $status = "Hanging in there"      if $meanPos > 47 ;
    $status = "Almost advancing"      if $meanPos > 53 ;
    $status = "Advancing slowly"      if $meanPos > 57 ;
    $status = "Advancing"             if $meanPos > 70 ;
    $status = "Advancing fast"        if $meanPos > 80 ;
    $status = "A ROCKET!"             if $meanPos > 90 ;

    #
    # Compute how many of users submits that are in the last 10%
    # (== he will soon lose the points)
    #
    my $troublePts = 0 ;
    # Contains percentage of points in last 10% of submits
    if(defined $ptsInLast10{$usr}) { 
	$troublePts = ($ptsInLast10{$usr}*100)/$subsByUsr{$usr} ; 
    } ;

    #
    # Add an extra text if user has a lot of points in last 10%
    #
    my $and = " and" ;
    $and = " but" if $meanPos > 43 ;
    my $tmp = "" ;
    $tmp = "$and in some trouble"                   if ($troublePts > 12) ;
    $tmp = "$and in trouble"                        if ($troublePts > 15);
    $tmp = "$and in <B>big</B> trouble"             if ($troublePts > 20);
    $tmp = "$and in <B><BIG>huge</BIG></B> trouble" if ($troublePts > 30);
    $status .=$tmp ;

    if($subsByUsr{$usr} != $prevUserPoints){
	$posInRace      = $posInRace + $usersAtSamePos;
	$prevUserPoints = $subsByUsr{$usr};
	$usersAtSamePos = 1;
	$pos            = $posInRace ;
    } else { 
	$usersAtSamePos = $usersAtSamePos + 1;
	$pos =" " ;
    } ;
    
    #
    # Treat the first three special
    #
    $pos eq "1" && do { 
	$pos="<BIG><FONT COLOR=\"red\"><$blink>First</$blink></FONT></BIG>"   ;
    };
    $pos eq "2" && do { 
	$pos="<BIG><FONT COLOR=\"blue\">Second</FONT></BIG>";
    };
    $pos eq "3" && do { 
	$pos="<BIG><FONT COLOR=\"blue\">Third</FONT></BIG>"  ;
    };

    #
    # End first table and start second if position is greater than 3
    #
    $posInRace ge "4" && ($table eq "first") && do {
	print
	    &P4CGI::end_table(),
	    "\n<B>Followed by:</B>\n",
	    &P4CGI::start_table("BORDER=3 BGCOLOR=\"#E0E0E0\""),
	    &P4CGI::table_row(-type=>"th",
			      "Position",
			      "User",
			      "# of submits",
			      "Comment") ;
	$table="second" ;
    } ;
    
    #
    # Translate user to "real name", if available
    #
    my $user = $usr ;
    if($userToName{$usr} && ($userToName{$usr} ne $usr)) {
	$user = "$userToName{$usr}" ;
    };
    
    #
    # Print table entry
    #
    my @bgcolor = () ;
    push @bgcolor,("-bgcolor","\"#99ff99\"") if  $usr eq $firstUsr; 
    $subsByUsr{$usr} = "<FONT COLOR=red>$subsByUsr{$usr}</FONT>" if $usr eq $lastUsr;
    print &P4CGI::table_row({-align=>"center",
			     -text=>$pos},
			    $user,
			    {-align=>"center",
			     @bgcolor,
			     -text=>$subsByUsr{$usr}},
			    $status) ;
};

#
# Print page end
#
print "",
    &P4CGI::end_table(),
    "\n<br><br><SMALL>This page is best viewed with $bestViewed</SMALL>",
    &P4CGI::end_page();


