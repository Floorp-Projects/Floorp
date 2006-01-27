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
#  List all labels
#
#################################################################

##
#
# Parameters:
#
# SORTBY defines sort order
#    One of "NAME" and "DATE"
#
#

my $SORTBY = &P4CGI::cgi()->param("SORTBY") ;
$SORTBY = "NAME" unless defined $SORTBY and $SORTBY eq "DATE" ;

# Get list of all label
my @labels ;
&P4CGI::p4call(\@labels, "labels" );

map { /^Label (\S+)\s+(\S+)\s+'(.*)'/ ; $_ = [$1,$2,$3] ; } @labels ;

# Print header
my @legend = ("<b>label</b> -- view label info") ;

my @lab ;
if($SORTBY eq "DATE") {
    @lab = sort { my @b = @$a ;
		  my @a = @$b ;
		  $a[1] cmp $b[1] ; } @labels ;
    push @legend,&P4CGI::ahref(-url => "labelList.cgi",
			       "SORTBY=NAME",
			       "Sort list by name") ;

}
else {
    @lab = sort { my @a = @$a ;
		  my @b = @$b ;
		  uc($a[0]) cmp uc($b[0]) ; } @labels ;
    push @legend,&P4CGI::ahref(-url => "labelList.cgi",
			       "SORTBY=DATE",
			       "Sort list by date") ;
}


print "",
    &P4CGI::start_page("List of labels",
		       &P4CGI::ul_list(@legend)) ;
print "",
    scalar @labels," labels",
    &P4CGI::start_table(""),
    &P4CGI::table_header("Label/label info","Date","Desc.") ;

foreach (@lab) {    
    my ($name,$date,$desc) = @{$_} ;
    my $lab = 
    print &P4CGI::table_row(-valign => "top",
			    &P4CGI::ahref(-url => "labelView.cgi",
					  "LABEL=$name",
					  $name),
			    $date,
			    $desc) ;
}

print 
    &P4CGI::end_table(),
    &P4CGI::end_page() ;

#
# That's all folks
#
