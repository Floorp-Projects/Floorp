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
#  P4 depot tree browser
#
#################################################################

###
### Handle file spec argument
###

my $err2null = &P4CGI::REDIRECT_ERROR_TO_NULL_DEVICE() ;
my $ROOT = &P4CGI::root();

# * Get path from argument
my $fspc = P4CGI::cgi()->param("FSPC") ;
&P4CGI::bail("Invalid file spec.") if ($fspc =~ /[<>"&:;'`]/);
				
# canonicalize to either "/.../" or "/"
$fspc =~ s/\.\.\.$// if defined $fspc ;
$fspc = $ROOT unless defined $fspc ;
$fspc = "/$fspc/" ;

while($fspc =~ s|//|/|) {} ;
				# Find out if we are at root
my $weAreAtROOT = ($fspc eq $ROOT) ;

###
### handle "Hide deleted files" argument
###

				# * Get HIDEDEL argument (Hide deleted files)
my $hidedel = P4CGI::cgi()->param("HIDEDEL") ;
$hidedel = "NO" unless defined $hidedel ;
$hidedel = "YES" unless $hidedel eq "NO" ;

my $p4DirsDOption = "" ;	# Set -D option for "p4 dirs" if hide deleted
$p4DirsDOption = " -D" if $hidedel eq "NO" ;

###
### Figure out "back" buttons
###
my @back ;

my $tmp = "$fspc" ;		# Copy arg
$tmp =~ s|([^/]+)/$|| ;		# Remove last subdir
unshift @back,"/".$1;				# 

while($tmp ne $ROOT) {
    $tmp =~ s|([^/]+)/$|| or last ;
    my $f = $1 ;
    unshift @back,&P4CGI::ahref("FSPC=$tmp$f",
				"HIDEDEL=$hidedel",
				"/$f") ;
} ;
unless($weAreAtROOT) {
    unshift @back,&P4CGI::ahref("FSPC=$ROOT",
				"HIDEDEL=$hidedel",
				"[ROOT]") ;
}


###
### Create link to changes for all files below
###
my $linkToAllbelow = &P4CGI::ahref(-url => "changeList.cgi",
				  "CMD=changes",
				  "FSPC=/$fspc...",
				   "View changes") ;
###
### Create link to view changes for a specific user below this point
###
my $linkToChangeByUser = &P4CGI::ahref(-url => "changeByUsers.cgi",
				       "FSPC=/$fspc...",
				       "View changes by user or group") ;

###
### Create link to search for pattern
###
my $linkToPatternSearch = &P4CGI::ahref(-url => "searchPattern.cgi",
				       "FSPC=/$fspc...",
				       "Search for pattern in change descriptions") ;

###
### Create link to recently modified files
###
my $recentlyModified = &P4CGI::ahref(-url => "filesChangedSince.cgi",
				     "FSPC=/$fspc...",
				     "Recently modified files") ;


###
### Create link to depot statistics
###
my $depotStatistics  = &P4CGI::ahref(-url => "depotStats.cgi",
				     "FSPC=/$fspc...",
				     "Depot Statistics") ;


###
### Get subdirs
###
my @subdirs ;
&P4CGI::p4call(\@subdirs,"dirs $p4DirsDOption \"/$fspc*\" $err2null") ;
map  { my $dir = $_ ;
       my $dirname ;
       ($dirname = $dir) =~ s|^.*/|/| ;
       $_ = P4CGI::ahref("FSPC=$dir",
			 "HIDEDEL=$hidedel",
			 $dirname) ;
   } @subdirs ;

###
### Get files
###
my @files ;
my @tmp ;
&P4CGI::p4call(\@tmp,"files \"/$fspc*\" $err2null") ;
@files = map  { /([^\#]+)\#(.*)/ ;
		my $file=$1 ;
		my $info=$2 ;
		$file =~ s/^.*\/// ;
		my ($rev,$inf) = split / - /,$info ;
		my $pfile = "$file" ;
		my $prev ;
		if($inf =~ /^delete/) {
		    $prev = "<STRIKE>$rev</STRIKE>";
		    if($hidedel eq "YES") { 
			$pfile = undef ;
		    }
		    else {
			$pfile= "<STRIKE>$file</STRIKE>";
		    }
		} 
		else {
		    $prev = &P4CGI::ahref(-url => "fileViewer.cgi",
					  "FSPC=/$fspc$file",
					  "REV=$rev",
					  "$rev") ;
		};
		if($pfile) {
		    $pfile = &P4CGI::ahref(-url => "fileLogView.cgi",
					   "FSPC=/$fspc$file",
					   "$pfile").
					       "<font color=#808080>&nbsp;\#</font>$prev" ;
		} ;
		defined $pfile?$pfile:() ;
	    } @tmp ;

###
### Create link for "hide/view deleted files" 
###
my $toggleHide ;
if($hidedel eq "YES") {
    $toggleHide = P4CGI::ahref("FSPC=/$fspc",
			       "HIDEDEL=NO",
			       "Show deleted files") ;
}
else {
    $toggleHide = P4CGI::ahref("FSPC=/$fspc",
			       "HIDEDEL=YES",
			       "Hide deleted files") ;
}

###
### Set help target
###
&P4CGI::SET_HELP_TARGET("depotTreeBrowser") ;

###
### Start page printout
###
print
    "",
    &P4CGI::start_page("Depot Tree Browser",
		       &P4CGI::ul_list("<b>Subdir</b> -- Descend to subdir",
				       "<b>File</b> -- Show file log",
				       "<b>Rev</b> -- View current revision",
				       "$toggleHide")) ;


my $sarg=$weAreAtROOT?"[ROOT]":"/$fspc" ; # replace // with [ROOT]

				# Print current directory
print "<H2 align=center><TT>".join("",@back)."</TT></H2>" ;

				# Print "back buttons"
if(@back > 0) {
    print &P4CGI::image("back.gif")," Back to: ", join(' ',@back) ;
}

###
# Make table with three columns
#
sub makeThreeColumns(@)
{
    my $l = @_ ;
    my $len = int((@_+2)/3) ;    
    while(@_ < ($len*3)) { push @_,"" ;} ; # To avoid error messages
    return join("\n",(&P4CGI::start_table(" COLS=4 width=100%"),
		      &P4CGI::table_row({-valign => "top",
					 -width => "10",
					 -text => ""},
					{-valign => "top",
					 -text => join("<br>\n",@_[0..$len-1])},
					{-valign => "top",
					 -text => join("<br>\n",@_[$len..(2*$len)-1])},
					{-valign => "top",
					 -text => join("<br>\n",@_[(2*$len)..(3*$len)-1])}),
		      &P4CGI::end_table())) ;
}

if ($fspc eq "/") {
    print "<P><b>Depots</b>\n" ;
    if(@subdirs>0) {
	print makeThreeColumns(@subdirs) ;
    }    
}
else {
    print "<P><b>Subdirs</b>\n" ;
    if(@subdirs>0) {
	print makeThreeColumns(@subdirs) ;
    }
    else {
	print "<br>[No more subdirectories]" ;
    }
    print "<P><b>Files</b>\n" ;
    if(@files>0) {
	print makeThreeColumns(@files) ;
    }
    else {
	print "<br>[No files in this directory]<br>" ;
    }
} ;

print "<hr>\n" ;

print &P4CGI::start_table("bgcolor=".
			  &P4CGI::HDRFTR_BGCOLOR().
			  " align=center cellpadding=0 cellspacing=2") ;
print &P4CGI::table_row(-align=>"right",
			"$linkToAllbelow...") ;
print &P4CGI::table_row(-align=>"right",
			"$linkToChangeByUser...") ;
print &P4CGI::table_row(-align=>"right",
			"$linkToPatternSearch...") ;
print &P4CGI::table_row(-align=>"right",
			"$recentlyModified...") ;
print &P4CGI::table_row({-align=>"right",
			-text => "$depotStatistics..."},
			"...for <tt>/$fspc...</tt> :") ;

print &P4CGI::end_table() ;

print
    "",
    &P4CGI::end_page() ;


#
# That's all folks
#
