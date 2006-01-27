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
#  View labels
#
#################################################################

# Get label
my $label = P4CGI::cgi()->param("LABEL") ;
&P4CGI::bail("No label specified") unless defined $label ;
&P4CGI::bail("Invalid label.") if ($label =~ /[<>"&:;'`]/);

my $found ;
# Get list of all labels and also check that supplied label exists
my @labels ;
&P4CGI::p4call(\@labels, "labels" );
foreach (@labels) {
    $_ =~ s/^Label (\S+).*$/$1/ ;
    if($_ eq $label) {
	$found = "Yes" ;
    } ;
}

# Print header
print "",
    &P4CGI::start_page("Label $label",
		       &P4CGI::ul_list("<b>owner</b> -- view user info",
				       "<b>view</b> -- View changes for view")) ;

&P4CGI::signalError("Label $label not in depot") unless $found ;

my @otherLabels ;
foreach (@labels) {    
    next if ($_ eq $label) ;
    push @otherLabels,$_ ;
} ;

###
### "Sort" otherlabels after "closeness"
###
{
    my $lab=uc($label) ;
    my $len = length($lab) ;
    my @labs ;
    while($len > 3) {
	my @tmp ;
	my $l ;
	$len-- ;
	$lab = substr($lab,0,$len) ;
	foreach $l (@otherLabels) {
	    if(uc(substr($l,0,$len)) eq $lab) {
		push @labs,$l ;
	    }
	    else {
		push @tmp,$l ;
	    }
	}
	@otherLabels = @tmp ;	
    } ;
    @otherLabels = (@labs,@otherLabels) ;
}

    
# Get label info
print 
    "",
    &P4CGI::start_table("") ;


my %values ;
my @fields = &P4CGI::p4readform("label -o \"$label\"",\%values) ;

				# Fix description field
if(exists $values{"Description"}) {
    my $d = $values{"Description"} ;
    $values{"Description"} = "<pre>$d</pre>" ;
}
				# Fix owner field
if (exists $values{"Owner"}) {
    # Get real user names...
    my %userCvt ;
    {
	my @users ;
	&P4CGI::p4call(\@users, "users" );
	%userCvt =  map { /^(\S+).*> \((.+)\) .*$/ ; ($1,$2) ; } @users ;
    }

    my $u = $values{"Owner"} ;
    if(exists $userCvt{$u}) {
	$values{"Owner"} = &P4CGI::ahref(-url=>"userView.cgi",
					 "USER=$u",
					 "$u") . " (" . $userCvt{$u} . ")" ; 
    }
    else {
	$values{"Owner"} = "$u (Unknown user)" ;
    }
}
				# Fix view field
my $viewFSPC = $values{"View"} ;
my $view ;
foreach (split("\n",$values{"View"})) {
    my $t = &P4CGI::ahref(-url => "depotTreeBrowser.cgi",
			  "FSPC=$_",
			  "<tt>$_</tt>") ;
    if (defined $view) {
	$view .= "<br>$t" ;
    } 
    else {
	$view .= "$t" ;
    } ;
} ;
$values{"View"} = $view ;

print  &P4CGI::start_table("") ;

my $f ;
foreach $f (@fields) {
    print &P4CGI::table_row({-align => "right",
			     -valign => "top",
			     -type  => "th",
			     -text  => "$f"},
			    $values{$f}) ;
} ;


$viewFSPC =~ s/\n//ig ;



print    
    &P4CGI::table_row(undef,		      
		      &P4CGI::ul_list(&P4CGI::ahref(-url => "changeList.cgi",
						    "FSPC=$viewFSPC",
						    "EXLABEL=$label",
						    "List changes in label view not included in label"),
				      &P4CGI::ahref(-url => "changeList.cgi",
						    "LABEL=$label",
						    "View changes for label $label"),
				      &P4CGI::ahref(-url => "fileSearch.cgi",
						    "LABEL=$label",
						    "List files in label $label"))) ;

print &P4CGI::end_table() ;

print 
    "<hr>",
    &P4CGI::cgi()->startform(-action => "labelDiffView.cgi" ,
			     -method => "GET"),
    &P4CGI::cgi()->hidden(-name=>"LABEL1",
			  -value=>"$label"),
    "Diff with label: ",	
    &P4CGI::cgi()->popup_menu(-name  => "LABEL2",
			      -value => \@otherLabels),
    "<br>Show files that: ",
    &P4CGI::cgi()->checkbox(-name    => "SHOWSAME",
			    -value   => "Y",
			    -label   => " are not modified"),
    &P4CGI::cgi()->checkbox(-name    => "SHOWNOTSAME",
			    -checked => "Y",
			    -value   => "Y",
			    -label   => " are modified"),
    &P4CGI::cgi()->checkbox(-name    => "SHOWDIFF",
			    -checked => "Y",
			    -value   => "Y",
			    -label   => " differ"),
    &P4CGI::cgi()->submit(-name  => "Go",
			  -value => "Go"),
    &P4CGI::cgi()->endform() ;


print
    "<hr>",
    &P4CGI::cgi()->startform(-action => "changeList.cgi",
			     -method => "GET"),	
    &P4CGI::cgi()->hidden(-name=>"LABEL",
			  -value=>"$label"),
    "View changes for label $label excluding label: ",	
    &P4CGI::cgi()->popup_menu(-name  => "EXLABEL",
			      -value => \@otherLabels),
    &P4CGI::cgi()->submit(-name  => "Go",
			  -value => "Go"),
    &P4CGI::cgi()->endform() ;

print
    "<hr>",
    &P4CGI::cgi()->startform(-action => "fileSearch.cgi",
			     -method => "GET"),
    &P4CGI::cgi()->hidden(-name=>"LABEL",
			  -value=>"$label"),
    &P4CGI::cgi()->submit(-name  => "ignore",
			  -value => "Search in label for:"),
    &P4CGI::cgi()->textfield(-name      => "FSPC",
			     -default   => "//...",
			     -size      => 50,
			     -maxlength => 256),    
    &P4CGI::cgi()->endform() ;

print 
    "",
    &P4CGI::end_page() ;

#
# That's all folks
#
