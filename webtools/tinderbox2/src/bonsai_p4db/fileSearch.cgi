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
#  P4 search for file
#  Search depot for files matching spec
#
#################################################################

# Get file spec argument
my $filespec = P4CGI::cgi()->param("FSPC") ;
$filespec = &P4CGI::extract_printable_chars($filespec);
$filespec = "" unless defined $filespec ;
&P4CGI::bail("Invalid file spec.") if ($filespec =~ /[<>"&:;'`]/);

my $label = P4CGI::cgi()->param("LABEL") ;
if(!defined $label) {
    $label = "" ;
}
&P4CGI::bail("Invalid label.") if ($label =~ /[<>"&:;'`]/);

my $filedesc ;
my $showDiffSelection="Y" ;
if($filespec eq "") {
    $filedesc = " <small>label</small><br><code>$label</code>" ;
    $showDiffSelection= undef ;
}
else {
    $filedesc = "<br><code>$filespec</code>" ;
    if($label ne "") {
	$filedesc .= "<br><small>in label</small><br><code>$label</code>" ;
    }
}
if($label ne "") {
    $label = "\@$label" ;
}
				# Add //... if not there
if($filespec !~ /^\/\//) {
    $filespec = "//...$filespec" ;
}				
while($filespec =~ s/\.\.\.\.\.\./\.\.\./) { ; } ;
while($filespec =~ s/\*\*/\*/) { ; } ;

my $MAX_RESTART=100 ; # Restart table after this number of lines...

				# Check if file exists
my @matches ;
&P4CGI::p4call(\@matches, "files \"$filespec$label\"" );

my $tableStart ;
$tableStart = &P4CGI::start_table("cellpadding=1") . 
    "\n" .
    &P4CGI::table_header("Rev/view","Action/diff","Change/view ch.","File/view log");

my $tableSize = 0 ;


print "",
    &P4CGI::start_page("<small>Search result for</small>$filedesc",
		       &P4CGI::ul_list("<b>Filename</b> -- see the complete file history",
				       "<b>Revision Number</b> -- see the file text",
				       "<b>Action</b> -- see the deltas (diffs)",
				       "<b>Change</b> -- see the complete change description, including other files")) ;

    if(scalar(@matches) == 0) {
	print  "<font color=red>No files found matching $filespec</font>\n" ;
    }
    else {
	print "<font color=green>",scalar(@matches)," files found:</font>" ;
	if(@matches > $MAX_RESTART) {
	    my $n = 2 ;		# Compute a value for $MAX_RESTART that does not leave widows..
	    while(@matches/$n > $MAX_RESTART) { $n++ ; } ;
	    $MAX_RESTART = int(@matches/$n) ;
	}  ;
	my $f ;
	foreach $f (@matches) {
	    $f =~ /([^\#]+)\#(\d+) - (\w+) change (\d+)/ ;
	    my ($name,$rev,$act,$change)=($1,$2,$3,$4) ;
	    print $tableStart if $tableSize == 0 ;
	    $tableSize++ ;
	    print 
		"",
		&P4CGI::table_row(&P4CGI::ahref("-url","fileViewer.cgi",
						"FSPC=$name",
						"REV=$rev",
						$rev),
				  &P4CGI::ahref("-url","fileDiffView.cgi",
						"FSPC=$name",
						"REV=$rev",
						"ACT=$act",
						$act),
				  &P4CGI::ahref("-url","changeView.cgi",
						"CH=$change",
						$change),
				  &P4CGI::ahref("-url","fileLogView.cgi",
						"FSPC=$name",
						$name)) ;
	    if($tableSize > $MAX_RESTART) {
		print "",&P4CGI::end_table() ;
		$tableSize = 0 ;		
	    } ;
	}  ;
	print "",&P4CGI::end_table() if $tableSize > 0 ;
	my @files ;
	my %filesToFiles ;
	foreach $f (@matches) {
	    $f =~ /([^\#]+)\#(\d+) - (\w+) change (\d+)/ ;
	    my ($name,$rev,$act,$change)=($1,$2,$3,$4) ;
	    if($act ne "delete"){
		push @files,"$name\#$rev" ;
	    }
	} ;
	if(defined $showDiffSelection and @files > 1) {
	    print 
		&P4CGI::cgi()->startform("-action","fileDiffView.cgi",
					 "-method","GET"),
		&P4CGI::cgi()->hidden("-name","ACT",
				      "-value","edit"),
		"View diff between:<br>",
		&P4CGI::cgi()->popup_menu(-name => "FSPC",
					  -values => \@files),
		"and<br>",
		&P4CGI::cgi()->popup_menu(-name => "FSPC2",
					  -values => \@files),
		&P4CGI::cgi()->submit("-name","ignore",
				      "-value"=>"Go"),
		&P4CGI::cgi()->endform() ;
	} ;	
    } ;
print
    "",    
    &P4CGI::end_page() ;

#
# That's all folks
#
