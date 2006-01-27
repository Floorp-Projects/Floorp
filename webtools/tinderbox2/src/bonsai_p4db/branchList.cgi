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
#  List all branches
#
#################################################################

my $SORTBY=&P4CGI::cgi()->param("SORTBY") ;
$SORTBY="DATE" unless ($SORTBY == 'NAME');

my $otherSort ;
if($SORTBY eq "NAME") {
    $otherSort = &P4CGI::ahref("SORTBY=DATE",
			       "Sort by date") ;
}
else {
    $otherSort = &P4CGI::ahref("SORTBY=NAME",
			       "Sort by name") ;
} ;

# Print header
print "",
    &P4CGI::start_page("List of branches",
		       &P4CGI::ul_list("<b>Name</b> -- view branch info",
				       $otherSort)) ;

# Get list of all brances

local *P4 ;
print "",
    &P4CGI::start_table(""),
    &P4CGI::table_header("Name/view branch","Updated","Description") ;

&P4CGI::p4call(*P4, "branches" );
my %rows ;
while(<P4>) {
    if(/^Branch (\S+)\s+(\S+)\s+\'([^\']*)\'/) {
	my ($name,$cdate,$desc) = ($1,$2,$3) ;
	$name = &P4CGI::ahref(-url => "branchView.cgi" ,
			      "BRANCH=$name",
			      $name) ;
	my $s = $SORTBY eq "NAME"? uc("$name $cdate") : "$cdate $name" ;
	$rows{$s} = &P4CGI::table_row($name,$cdate,$desc) ;
    }
}
my $s ;
if($SORTBY eq "NAME") {
    foreach $s (sort keys %rows) {
	print $rows{$s} ;
    }
}
else {
    foreach $s (sort { $b cmp $a } keys %rows) {
	print $rows{$s} ;
    }
} ;
print "",
    &P4CGI::end_table(""),
    &P4CGI::end_page("") ;
#
# That's all folks
#
