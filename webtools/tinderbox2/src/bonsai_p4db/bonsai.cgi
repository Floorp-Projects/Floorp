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
#  Search changes for pattern
#
#################################################################

my $FSPC = &P4CGI::cgi()->param("FSPC") ;
$FSPC = "//..." unless defined $FSPC ;
&P4CGI::bail("Invalid file spec.") if ($FSPC =~ /[<>"&:;'`]/);

my @legend ;

my $legend = "" ;
$legend = &P4CGI::ul_list(@legend) if @legend > 0 ;


my $FSPC = P4CGI::cgi()->param("FSPC") || "//..." ;
$FSPC = P4CGI::extract_filename_chars($FSPC);

my $COMPLETE= &P4CGI::cgi()->param("COMPLETE") ;
if (defined($COMPLETE)) {
     $COMPLETE ="Yes";
}

unless(defined $COMPLETE) {
    push @legend,&P4CGI::ahref("COMPLETE=Yes",
			       "FSPC=$FSPC",
			       "Include old users in list") ;
} ;

$legend = '';
print "",   &P4CGI::start_page("General Perforce Query Fourm",$legend) ;





# Get users
my @users ;
@users=&P4CGI::run_cmd("users");
my @listOfUsers = sort { uc($a) cmp uc ($b) } map { /^(\S+).*> \((.+)\) .*$/ ; $1 ; } @users ;
my %userCvt =  map { /^(\S+).*> \((.+)\) .*$/ ; ($1,$2) ; } @users ;
@listOfUsers = ('', @listOfUsers);  

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

&P4CGI::SET_HELP_TARGET("searchPattern") ;


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

my %allUsers;


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
	       
my $SHOWFILES= &P4CGI::cgi()->param("SHOWFILES") || "";
my $DATESPECIFIER= &P4CGI::cgi()->param("DATESPECIFIER") ;

my $browse_checked = '';
my $picklist_checked = '';
my $explicit_checked =  '';

if ($DATESPECIFIER eq 'browse') {
    $browse_checked = 'CHECKED';
} elsif ($DATESPECIFIER eq 'picklist') {
    $picklist_checked = 'CHECKED';
} elsif ($DATESPECIFIER eq 'explicit') { 
    $explicit_checked = 'CHECKED';
} else {
    # default value if none checked
    $picklist_checked = 'CHECKED';

}

prSelection("changeList.cgi",
	    "General Perforce Query Fourm",
	    join("\n",(&P4CGI::start_table(),

		       "<tr>",
		       "<td align=right valign=center>File spec:</td>",
		       "<td align=right valign=center></td>",
		       "<td align=left valign=center><font face=fixed>",
		       &P4CGI::cgi()->textfield(-name      => "FSPC",
						-default   => "//...",
						-size      => 50,
						-maxlength => 256),		       
		       "</font></td></tr>",

		       "<tr>",
		       "<td align=right valign=center>Browse</td>",
                       "<td><input type='radio' name='DATESPECIFIER' value='browse' $browse_checked></td>",
		       "<td align=left valign=center><font face=fixed>",
 		       "</font></td></tr>",

		       "<tr>",
		       "<tr>",
		       "<td align=right valign=center>Changes within the last:</td>",
		       "<td><input type='radio' name='DATESPECIFIER' value='picklist' $picklist_checked></td>",
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
		       "<td align=right valign=center>Between</td>",
                       "<td><input type='radio' name='DATESPECIFIER' value='explicit' $explicit_checked></td>",
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
		       "<td align=right valign=center>Description Pattern:</td>",
		       "<td align=right valign=center></td>",
		       "<td align=left valign=center><font face=fixed>",
		       &P4CGI::cgi()->textfield(-name      => "SEARCHDESC",
						-default   => "<pattern>",
						-size      => 50,
						-maxlength => 256),
		       "</font></td></tr>",

		       "<td align=right valign=center>Invert search:</td>",
		       "<td align=right valign=center></td>",
		       "<td align=left valign=center><font face=fixed>",
		       &P4CGI::cgi()->checkbox(-name    => "SEARCH_INVERT",
					       -value   => 1,
					       -label   => " Search descriptions <B>NOT</B> including pattern"),
		       "</font></td></tr>",

		       "<td align=right valign=center>Show files:</td>",
		       "<td align=right valign=center></td>",
		       "<td align=left valign=center><font face=fixed>",
		       &P4CGI::cgi()->checkbox(-name    => "SHOWFILES",
					       -value   => $SHOWFILES,
					       -label   => " Show the files for each change"),
		       "</font></td></tr>",


		       "<td align=right valign=center>User(s):</td>",
		       "<td align=right valign=center></td>",
		       "<td align=left valign=center><font face=fixed>",
		       &P4CGI::cgi()->scrolling_list(-name      => "USERS",
						     -values    => \@listOfUsers,
						     -size      => $ulistSize,
						     -multiple  => 'true',
						     -labels    => \%userCvt) .
		       "</font>",
		       "</font></td></tr>",


		       "<td align=right valign=center>Group(s):</td>",
		       "<td align=right valign=center></td>",
		       "<td align=left valign=center><font face=fixed>",
		       &P4CGI::cgi()->scrolling_list(-name      => "GROUPS",
						     -values    => \@listOfgroups,
						     -size      => $glistSize,
						     -multiple  => 'true') .
		       "</font>",
		       "</font></td></tr>",


		       &P4CGI::end_table())),
		 "searchPatt") ;
	    
	    print &P4CGI::end_table() ;
	    

print  "</tr></td>",&P4CGI::end_table() ;

print    
    &P4CGI::end_page() ;

#
# That's all folks
#

