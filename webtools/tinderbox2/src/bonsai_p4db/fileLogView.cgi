#!/usr/bin/perl -Tw
# -*- perl -*-

use lib '.';
use P4CGI ;
use strict ;
use CGI::Carp ;
#
#################################################################
#  CONFIGURATION INFORMATION 
#  All config info should be in P4CGI.pm
#
#################################################################
#
#  P4 file log viewer
#
#################################################################

sub offsetOf($@ ) {
    my $v = shift @_ ;
    my $pos = 0 ;
    while(@_ > 0) {
	if($v eq (shift @_)) {
	    return $pos ;
	}
	$pos++ ;
    }
    return -1 ;
}

my $err2null = &P4CGI::REDIRECT_ERROR_TO_NULL_DEVICE() ;
my $err2stdout =  &P4CGI::REDIRECT_ERROR_TO_STDOUT() ;

local *P ;

				# File argument
my $file = P4CGI::cgi()->param("FSPC") ;
$file = &P4CGI::extract_printable_chars($file);
&P4CGI::bail("No file spec") unless defined $file ;
&P4CGI::bail("Invalid file spec.") if ($file =~ /[<>"&:;'`]/);

				# Label x-reference argument
my $listLabel = P4CGI::cgi()->param("LISTLAB") ;
$listLabel = "No" unless defined $listLabel and $listLabel eq "Yes";

				# Show branch info argument
my $showBranch = P4CGI::cgi()->param("SHOWBRANCH") ;
$showBranch="No" unless defined $showBranch and $showBranch eq "Yes";

				# Get file data
my @filelog ;
@filelog=&P4CGI::run_cmd("filelog", $file) ;

&P4CGI::bail("No data for file \"$file\"") if @filelog == 0 ;

				# Get info about opened status
&P4CGI::p4call(*P,"opened -a \"$file\" $err2null") ;
my %opened ;
my $openedText = "" ;
while(<P>) {
    $openedText = "Opened by" ;
    chomp ;
    /\w+\#(\d+) - .* by (\w+)\@(\S+)/ or 
	&P4CGI::bail("Can not read info from \"p4 opened\"") ;
    my $user =  &P4CGI::ahref(-url => "userView.cgi",
			      "USER=$2",
			      "$2") ;
    
    my $client =  &P4CGI::ahref(-url => "clientView.cgi",
				"CLIENT=$3",
				"$3") ;
    
    if(exists $opened{$1}) {
	$opened{$1} .= "<br> and $user\@$client" ;
    } else {
	$opened{$1} = "$user\@$client" ;
    } ;
} ;
close *P ;


				# Get list of labels (if $listLabel is set)
my @labels ;
if($listLabel eq "Yes") {
    &P4CGI::p4call(*P,"labels") ;
    while(<P>) {
	/^Label (\S+)/ and do { push @labels,$1 ; } ;
    }
    close P ;
}
				# Create hash containing labels by file name and
				# version
my %fileToLabels ;
if(@labels > 0) {
				# Try to speed things up by looking up
				# file view for each label and removing all
				# labels that don't match
				#   This is an act of desperation because in our
				#   p4 depot the label search takes forever (well..
				#   a long time, 20 secs or so...)
    if(1) {
	my $l ;
	my @l ;
	LABEL: foreach $l (@labels) {
	    my %data ;
	    &P4CGI::p4readform("label -o \"$l\"",\%data) ;
	    if(exists $data{"View"}) {
		my @v = split("\n",$data{"View"}) ;
		foreach (@v) {
				# p4-to-perl regexp conversion
		    my $in = $_ ;
		    my $re = "" ;
		    while($in =~ s/(.*?)(\Q...\E|\Q*\E)//) {
			$re .= "\Q$1\E" ;
			if($2 eq "...") { $re .= ".*"    ; } 
			else            { $re .= "[^/]*" ; }
		    }
		    $re .= "\Q$in\E" ;
		    if($file =~ /$re/) {
			push @l,$l ;
			next LABEL ;
		    }
		} 
	    }
	}
	my $lb = @labels ;
	my $la = @l ;
	&P4CGI::ERRLOG("reduced from $lb to $la labels") ; # DEBUG
	@labels = @l ;
    }
				# <RANT>
				# Frankly, I find it very strange that I can speed
				# up the search by "manually" reading all label
				# specs, parsing them, and checking if the file
				# matches any part of the view before actually
				# asking p4 to do it. Some developer must have had
				# a bad day at perforce. And p4 is not open
				# source.... sigh.
				# </RANT>


    my $filelabels = "" ;
    foreach (@labels) {
	$filelabels .= " \"$file\@$_\"" ;
    }
    my @filesInLabels ;
    &P4CGI::p4call(\@filesInLabels,"files $filelabels $err2stdout") ;
    my $l ;
				# Remove labels not in list
				#   NOTE! The errors (file not in label-messages)
				#         are printed to stderr and there
				#         is no guarantee that output from stderr and
				#         stdout will come in order. This is why
				#         we first must figure out which labels
				#         that NOT affected the file
    foreach $l (reverse map {/.*@(\S+)\s.*not in label/?$1:()} @filesInLabels) {
	my $offset = offsetOf($l,@labels) ;
	splice @labels,$offset,1 ;	
    }
				# Build file-to-label hash. Use only data from
				# stdout (not stderr). (grep is used to filter)
    foreach (grep(!/not in label/,@filesInLabels)) {	
	my $lab = shift @labels ;
	/^(\S+)/ ;
	if(defined $fileToLabels{$1}) {
	    $fileToLabels{$1} .= "<br>$lab" ; 
	} 
	else {
	    $fileToLabels{$1} = "$lab" ; 
	}
    }
} ;


my @legendList ; 
push @legendList,
    "<b>Revision Number</b> -- see the file text",
    "<b>Action</b> -- see the deltas (diffs)",
    "<b>User</b> -- see info about user",
    "<b>Change</b> -- see the complete change description, including other files",
    &P4CGI::ahref("-url","changeList.cgi",
		  "FSPC=$file",
		  "Changes") . "-- see list of all changes for this file" ;

my @parsListLab ;
my @parsShowBranch ;
my $p ;
foreach $p (&P4CGI::cgi()->param()) {
    push @parsListLab, "$p=" . &P4CGI::cgi()->param($p) unless $p eq "LISTLAB" ;
    push @parsShowBranch, "$p=" . &P4CGI::cgi()->param($p) unless $p eq "SHOWBRANCH" ;
}

if($listLabel ne "Yes") {
    push @legendList,
    &P4CGI::ahref(@parsListLab,
		  "LISTLAB=Yes",
		  "List labels") . "-- list cross ref. for labels" ;
} ;
if($showBranch ne "No") {
    push @legendList,
    &P4CGI::ahref(@parsShowBranch,
		  "SHOWBRANCH=No",
		  "Hide branch info") . "-- hide info about branches, merges and copy of file" ;
} 
else {
    push @legendList,
    &P4CGI::ahref(@parsShowBranch,
		  "SHOWBRANCH=Yes",
		  "Show branch info") . "-- show info about branches, merges and copy of file" ;
} ;

				# Get file directory part
my $fileDir=$file ;
$fileDir =~ s#/[^/]+$## ;
push @legendList,
    &P4CGI::ahref("-url","depotTreeBrowser.cgi",
		  "FSPC=$fileDir",
		  "Browse directory") . 
    "-- Browse depot tree at $fileDir" ;

my @back = &P4CGI::back_buttons($file);
print "",&P4CGI::start_page("File log<br>@back",&P4CGI::ul_list(@legendList)) ;

my $labelHead ="";
if($listLabel eq "Yes") {
    $labelHead="In label(s)" ;
} ;

print
    "",
    &P4CGI::start_table(""),
    &P4CGI::table_header("Rev/view file",
			 "Action/view diff",
			 "Date",
			 "User/view user",
			 "Change/view change",
			 "Type",
			 "Desc",
			 $labelHead,
			 $openedText) ;

my $log ;
my @revs ;
my %relatedFiles ;
my ($rev,$change,$act,$date,$user,$client,$type,$desc) ;
my $chbuffer = "" ;
while($log = shift @filelog) {
    $_ = &P4CGI::fixSpecChar($log) ;
    if(/^\.\.\. \#(\d+) \S+ (\d+) (\S+) on (\S+) by (\S*)@(\S*) (\S*)\s*'(.*)'/ ) {
	print $chbuffer ;
	$chbuffer = "" ;
	($rev,$change,$act,$date,
	 $user,$client,$type,$desc) = ($1,$2,$3,$4,$5,$6,$7,$8) ;
	$type =~ s/\((.*)\)/$1/ ;
	$desc = &P4CGI::magic($desc) ;
	push @revs,$rev ;
	my $labels = $fileToLabels{"$file\#$rev"} ;
	$labels = "" unless defined $labels ;
	$labels = "<b>$labels</b>" ;
	if ($act eq 'branch') {
	    $chbuffer .= 
		&P4CGI::table_row(-valign => "top",
				  &P4CGI::ahref("-url","fileViewer.cgi",
						"FSPC=$file",
						"REV=$rev",
						"$rev"),
				  "$act",
				  "$date",
				  &P4CGI::ahref(-url => "userView.cgi" ,
						"USER=$user",
						"$user"),
				  &P4CGI::ahref("-url","changeView.cgi",
						"CH=$change",
						"$change"),
				  "$type",
				  "<tt>$desc</tt>",
				  $labels,
				  exists $opened{$rev}?$opened{$rev}:"") ;
	}
	elsif ($act eq 'delete') {
	    $chbuffer .= 
		&P4CGI::table_row(-valign => "top",
				  "$rev",
				  "<strike>delete</strike>",
				  "$date",
				  &P4CGI::ahref(-url => "userView.cgi" ,
						"USER=$user",
						"$user"),
				  &P4CGI::ahref("-url","changeView.cgi",
						"CH=$change",
						"$change"),
				  "$type",
				  "<tt>$desc</tt>",
				  $labels,
				  exists $opened{$rev}?$opened{$rev}:"") ;
	}
	else {
	    $chbuffer .= 
		&P4CGI::table_row(-valign => "top",
				  &P4CGI::ahref("-url","fileViewer.cgi",
						"FSPC=$file",
						"REV=$rev",
						"$rev"),
				  &P4CGI::ahref("-url","fileDiffView.cgi",
						"FSPC=$file",
						"REV=$rev",
						"ACT=$act",
						"$act"),				  
				  "$date",
				  &P4CGI::ahref(-url => "userView.cgi" ,
						"USER=$user",
						"$user"),
				  &P4CGI::ahref("-url","changeView.cgi",
						"CH=$change",
						"$change"),
				  "$type",
				  "<tt>$desc</tt>",
				  $labels,
				  exists $opened{$rev}?$opened{$rev}:"") ;
	}
    }
    else {
	if(/^\.\.\. \.\.\. (\w+) (\w+) (\S+?)\#(\S+)/) {
	    my ($op,$direction,$ofile,$orev) = ($1,$2,$3,$4) ;	
	    my $file = $ofile ;
	    $file =~ s/\#.*$// ; 
	    $relatedFiles{$file} = 1 ;
	    if($showBranch ne "No") {
		my ($b1,$b2) = ("","") ;
		if($op eq "copy") {
		    ($b1,$b2) = ("<b> ! ","</b>") ;
		}
		my $d = &P4CGI::table_row(-valign => "top",
					  "",
					  undef,
					  undef,
					  undef,
					  undef,
					  undef,
					  undef,
					  undef,
					  undef,
					  "$b1$op $direction ".
					  &P4CGI::ahref("-url","fileLogView.cgi",
							"FSPC=$ofile", 
							"$ofile\#$orev"). "$b2") ;
		
		if($direction ne "from") {
		    $chbuffer = "$d\n$chbuffer" ;
		}
		else {
		    print "$chbuffer\n$d\n" ;
		    $chbuffer = "" ;
		}
	    }
	}
    }
}
print "$chbuffer\n" ;

print
    "",
    &P4CGI::end_table("") ;

if(@revs > 2) {
    print
	"<hr>",
	&P4CGI::cgi()->startform("-action","fileDiffView.cgi",
				 "-method","GET"),
	&P4CGI::cgi()->hidden("-name","FSPC",
			      "-value",&P4CGI::fixspaces("$file")),    
	&P4CGI::cgi()->hidden("-name","ACT",
			      "-value","edit"),    
	"\nShow diff between revision: ",    
	&P4CGI::cgi()->popup_menu(-name   =>"REV",
				  "-values" =>\@revs);
    shift @revs ;
    print
	" and ",
	&P4CGI::cgi()->popup_menu(-name   =>"REV2",
				  "-values" =>\@revs),
	"   ",
	&P4CGI::cgi()->submit(-name  =>"Go",
			      -value =>"Go"),
	&P4CGI::cgi()->endform() ;
} ;

sub getRelatedFiles($ )
{
    my $file = shift @_ ;
    my @data ;
    &P4CGI::p4call(\@data,"filelog \"$file\"") ;
    my %res ;
    map  {  if(/^\.\.\. \.\.\. \w+ \w+ (\S+?)\#/) { $res{$1} = 1 ; } ; } @data  ;
    return ( sort keys %res ) ;
} ;


if((keys %relatedFiles) > 0) {
    my @rel = sort keys %relatedFiles ;
    my @fileLinks = map {  &P4CGI::ahref("-url","fileLogView.cgi",
					 "FSPC=$_", 
					 "$_") ; } @rel ;
    my %indrel ;
    $relatedFiles{$file} = 1 ;    
    while(@rel > 0) {
	my $r ;
	foreach $r (map { exists $relatedFiles{$_} ? () : $_ } getRelatedFiles(shift @rel)) {
	    &P4CGI::ERRLOG("found: $r") ;
	    $indrel{$r} = 1;
	    push @rel, $r ;
	    $relatedFiles{$r} = 1 ;
	}
    }

    my @indFileLinks = map {  &P4CGI::ahref("-url","fileLogView.cgi",
					    "FSPC=$_", 
					    "$_") ; } sort keys %indrel ;
    
    print
	"",
	&P4CGI::start_table(),
	&P4CGI::table_row({ -valign => "top",
			    -align => "right",
			    -type => "th",
			    -text => "Related files:" },
			  { -text => &P4CGI::ul_list(@fileLinks) }) ;
    if(@indFileLinks > 0) {
	print "", &P4CGI::table_row({ -valign => "top",
				      -align => "right",
				      -type => "th",
				      -text => "Indirect:" },
				    { -text => &P4CGI::ul_list(@indFileLinks) }) ;
    } ;
    print "", &P4CGI::end_table() ;
} ;						


print
    "",
    &P4CGI::end_page() ;

#
# That's all folks
#
