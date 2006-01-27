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
#  View files affected by a set of changes
#
#################################################################

my $err2null = &P4CGI::REDIRECT_ERROR_TO_NULL_DEVICE() ;

####
# Parameters
# FSPC = file spec
#
# NEWER_THAN = restrict to changes newer than specified No. of hours
#

my $FSPC = P4CGI::cgi()->param("FSPC") || "//..." ;
$FSPC = P4CGI::extract_filename_chars($FSPC);
my @FSPC = split(/\s*\+?\s*(?=\/\/)/,$FSPC) ;

my $WEEKS = P4CGI::cgi()->param("WEEKS") ;
if(defined $WEEKS) {
    $WEEKS = P4CGI::extract_digits($WEEKS) ;
} 
else {
    $WEEKS = 0 ;
}

my $DAYS = P4CGI::cgi()->param("DAYS");
if(defined $DAYS) {
    $DAYS = P4CGI::extract_digits($DAYS);
} 
else {
    $DAYS=0 ;
}

my $HOURS = P4CGI::cgi()->param("HOURS");
if(defined $HOURS) {
    $HOURS = P4CGI::extract_digits($HOURS);
} 
else {
    $HOURS = 0 ;
}
my $SUBMIT = P4CGI::cgi()->param("SUBMIT");
if(defined $SUBMIT) {
    $SUBMIT=1;
} else {
    $SUBMIT='';
}
    
my $seconds = 3600 * ( $HOURS + (24 * ($DAYS + (7 * $WEEKS)))) ;

my $MINDATE = P4CGI::cgi()->param("MINDATE") ;

my $MAXDATE = P4CGI::cgi()->param("MAXDATE") ;

my $DATESPECIFIER = P4CGI::cgi()->param("DATESPECIFIER") ;
my $TIMEINTERVALSTR;

if      ($DATESPECIFIER eq 'explicit') {
     $TIMEINTERVALSTR = "\@".&P4CGI::DateStr2Time($MINDATE).",\@".&P4CGI::DateStr2Time($MAXDATE);
} elsif ($DATESPECIFIER eq 'picklist') {
     $TIMEINTERVALSTR = &P4CGI::DateList2Time($WEEKS, $DAYS, $HOURS);
} else {
     $TIMEINTERVALSTR = '';
}

my %allUsers;

if (($SUBMIT) && ($seconds)) {
#
# get time strings to compare to
#
    my $time = time() ;
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($time);
    my $currentTimeString = sprintf("\@%d/%02.2d/%02.2d:%02.2d:%02.2d:%02.2d",
				    1900+$year,$mon+1,$mday,$hour,$min,$sec) ;
    
    ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($time - $seconds);
    my $afterTimeString = sprintf("\@%d/%02.2d/%02.2d:%02.2d:%02.2d:%02.2d",
				  1900+$year,$mon+1,$mday,$hour,$min,$sec) ;
    my $niceAfterTimeString = sprintf("%d/%02.2d/%02.2d %02.2d:%02.2d",
				      1900+$year,$mon+1,$mday,$hour,$min) ;
    
    &P4CGI::ERRLOG("currentTimeString: $currentTimeString") ;
    &P4CGI::ERRLOG("afterTimeString: $afterTimeString") ;


#
# Start page
#
    print 
	"",
	&P4CGI::start_page("Files matching<br><TT>".
			   join("<br></tt>or<tt><br>\n",@FSPC).
			   "</TT><br> changed after $niceAfterTimeString ","") ;

#
# Get list of files changed
#

    my %toRev ;
    my %mode ;
    
    foreach $FSPC (@FSPC) {
	my @files ;
  my @cmd = ("files","${FSPC}${TIMEINTERVALSTR}");
  @files = P4CGI::run_cmd(@cmd);
	map { chomp;
              s/\#(\d+) - (\S+).*$// ; 
	      $toRev{$_}=$1 ; 
	      $mode{$_} =$2 ;   } @files ;
    }
    my @affectedFiles = sort keys %toRev ;
    
#
# Get revision at start of interval
#
    my %fromRev ;
    my @filesToCheck = @affectedFiles ;
    while(@filesToCheck > 0) {
	my @files ;
	while(($#files) < 1000 and @filesToCheck > 0) {
            my $file = shift(@filesToCheck);
            chomp $file;
	    push @files, $file.$TIMEINTERVALSTR ;
	}
	my @res ;
	my @cmd = ("files",@files);
	@res = P4CGI::run_cmd(@cmd);
	map { chomp; s/\#(\d+) - .*// ; $fromRev{$_}=$1 } @res ;
    }
    if(@affectedFiles == 0) {
	print "<font size=+1 color=red>No files found</font>\n" ;
    }
    else {
	print scalar @affectedFiles," files found<br>\n" ;
	

	print
	    "",
	    &P4CGI::start_table(""),
	    &P4CGI::table_header("From/view",
				 "/Diff",
				 "To/view",
				 "File/View file log",
				 "Change(s)/View change",
         "Users") ;
	
	my $f ;
	foreach $f (@affectedFiles) {

	    my @tmp ;
	    my $changes ;
      my $users ;
	    chomp $f;
	    my @cmd = ("changes","$f${TIMEINTERVALSTR}");
	    @tmp = P4CGI::run_cmd(@cmd);
	
      map { chomp;   
		/^Change (\d+).*$/ ;
 		my $c = &P4CGI::ahref(-url => "changeView.cgi",
				      "CH=$1",
				      $1) ;
		if(defined $changes) {
		    $changes .= ", $c" ;
		}
		else {
		    $changes = "$c" ;
		} ;
	    } @tmp ;


    map { chomp;
    / by ([^@]+)@/ ;
    my $user = $1;
    my $u = &P4CGI::ahref("-url" => "userView.cgi",
              "USER=$user",
              $user) ;

    if(defined $users) {
       $users .= ", $u" ;
    }
    else {
        $users = "$u" ;
    } ;


    $allUsers{$user} = 1 ;
                       
      } @tmp ;


	    my $file = &P4CGI::ahref(-url => "fileLogView.cgi",
				     "FSPC=$f",
				     $f) ;
	    my $fromRev ;
	    my $diff ;
	    if(exists $fromRev{$f}) {
		$fromRev = &P4CGI::ahref(-url => "fileViewer.cgi",
					 "FSPC=$f",
					 "REV=$fromRev{$f}",
					 $fromRev{$f}) ;	
		$diff = &P4CGI::ahref(-url => "fileDiffView.cgi",
				      "FSPC=$f",
				      "REV=$fromRev{$f}",
				      "REV2=$toRev{$f}",
				      "ACT=$mode{$f}",
				      "<font size=1>(diff)</font>") ;	
	    }
	    else {
		$fromRev = "" ;
		$diff = "<font size=-1 color=red>New</font>" ;
	    } ;
	    my $toRev ;
	    if($mode{$f} eq "delete") {
		$toRev = $toRev{$f} ;
		$diff = "<font size=-1 color=red>Deleted</font>" ;
	    }
	    else {
		$toRev = &P4CGI::ahref(-url => "fileViewer.cgi",
				       "FSPC=$f",
				       "REV=$toRev{$f}",
				       $toRev{$f}) ;
	    } ;
	    print &P4CGI::table_row(-align => "center",
				    $fromRev,
				    $diff,
				    $toRev,
				    {-align=>"left",
				     -text => $file},
				    {-align=>"left",
				     -text => $changes},
            {-align=>"left",
             -text => $users},
                                  
             ) ;
	} ;
    } ;
    print "", &P4CGI::end_table(),"<hr>" ;
    
}
else {
    print 
	"",
	&P4CGI::start_page("View recently changed files","") ;
    
} ;

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
				 -text =>  &P4CGI::cgi()->submit(-name  => "SUBMIT",
								 -value => "GO!")
				 },
				{ -valign=>"bottom",
				  -text => &P4CGI::cgi()->endform()
				  },
				) ;
} ;

print "",  &P4CGI::start_table("width=100% cellspacing=5") ;


my %dayValues = ( 0 => "Zero days",
		  1 => "One day",
		  2 => "Two days",
		  3 => "Three days",
		  4 => "Four days",
		  5 => "Five days",
		  6 => "Six days") ;

my %hourValues = ( 0 => "Zero hours",
		   1 => "One hour",
		   2 => "Two hours",
		   3 => "Three hours",
		   4 => "Four hours",
		   5 => "Five hours",
		   6 => "Six hours",
		   7 => "Seven hours",
		   8 => "Eight hours",
		   9 => "Nine hours") ;
{
    my $n = 9 ;
    while($n++ < 24) {
	$hourValues{$n} = "$n hours" ;
    }
}

my %weekValues = ( 0 => "Zero weeks",
		   1 => "One week",
		   2 => "Two weeks",
		   3 => "Three weeks",
		   4 => "Four weeks",
		   5 => "Five weeks",
		   6 => "Six weeks",
		   7 => "Seven weeks",
		   8 => "Eight weeks",
		   9 => "Nine weeks") ;
{
    my $n = 9 ;
    while($n++ < 24) {
	$weekValues{$n} = "$n weeks" ;
    }
}


my @dayValues = sort { $a <=> $b } keys %dayValues ;
my @hourValues = sort { $a <=> $b } keys %hourValues ;
my @weekValues = sort { $a <=> $b } keys %weekValues ;
	       
prSelection("filesChangedSince.cgi",
	    "List recently changed files",
	    join("\n",(&P4CGI::start_table(),
		       "<tr>",
 		       "<td align=right valign=center>File spec:</td>",
		       "<td align=right valign=center></td>",
		       "<td align=left valign=center><font face=fixed>",
		       &P4CGI::cgi()->textfield(-name      => "FSPC",
						-default   => $FSPC,
						-size      => 50,
						-maxlength => 256),		       
		       "</font></td></tr>",

		       "<tr>",
		       "<td align=right valign=center>Changes within the last:</td>",
		       "<td><input type=radio name=DATESPECIFIER value=picklist CHECKED></td>",
		       "<td align=left valign=center><font face=fixed>",
		       &P4CGI::cgi()->popup_menu(-name      => "HOURS",
						 -default   => 0,
						 -values    => \@hourValues,
						 -labels    => \%hourValues),
		       " <br>",
		       &P4CGI::cgi()->popup_menu(-name      => "DAYS",
						 -default   => 1,
						 -values    => \@dayValues,
						 -labels    => \%dayValues),
		       " <br>",
		       &P4CGI::cgi()->popup_menu(-name      => "WEEKS",
						 -default   => 0,
						 -values    => \@weekValues,
						 -labels    => \%weekValues),
		       "</font></td></tr>",

		       "<tr>",
		       "<td align=right valign=center>Between</td>",
                       "<td><input type=radio name=DATESPECIFIER value=explicit></td>",
		       "<td align=left valign=center><font face=fixed>",
 		       &P4CGI::cgi()->textfield(-name      => "MINDATE",
						-default   => "2005/03/27 18:13:00",
						-size      => 25,
						),		       
                       "</font></td></tr>",
                       "<td align=right valign=center>and</td>",
		       "<td align=right valign=center></td>",
		       "<td align=left valign=center><font face=fixed>",
		       &P4CGI::cgi()->textfield(-name      => "MAXDATE",
						-default   => &P4CGI::DateStr2Time("now"),
						-size      => 25,
						),		       
		       "</font></td></tr>",
		       "<tr>",
		       "</table>")),
	    "searchPatt") ;
		       
print &P4CGI::end_table() ;
		
print  "</tr></td>",&P4CGI::end_table() ;
print  &P4CGI::ahref(-url => "mailto:".join( ',', (sort keys %allUsers)) ,
                           ( "Mail everyone on this page". 
                             " (" . scalar(keys %allUsers) . " people)")
                             
                             ) ;

print &P4CGI::end_page() ;

#
# That's all folks
#




