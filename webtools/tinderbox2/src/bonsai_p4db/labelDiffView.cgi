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
#  P4 label diff viewer
#  View diff between two labels
#
#################################################################

# Get arguments

				# Labels to diff
my $LABEL1 = P4CGI::cgi()->param("LABEL1") ;
my $LABEL2 = P4CGI::cgi()->param("LABEL2") ;

&P4CGI::error("No first label specified") unless defined $LABEL1 ;
&P4CGI::error("No second label specified") unless defined $LABEL2 ;

&P4CGI::bail("Invalid first label.") if ($LABEL1 =~ /[<>"&:;'`]/);
&P4CGI::bail("Invalid second label.") if ($LABEL2 =~ /[<>"&:;'`]/);

				# defined if files that are the same in both labels
				# should be listed
my $SHOWSAME = P4CGI::cgi()->param("SHOWSAME") ;
if(defined $SHOWSAME) { undef $SHOWSAME if $SHOWSAME ne "Y" ; } ;

				# defined if files that are not the same in botha labels
				# should be listed
my $SHOWNOTSAME = P4CGI::cgi()->param("SHOWNOTSAME") ;
if(defined $SHOWNOTSAME) { undef $SHOWNOTSAME if $SHOWNOTSAME ne "Y" ; } ;

				# defined if files that exists only in one of the labels
				# shold be displayed
my $SHOWDIFF = P4CGI::cgi()->param("SHOWDIFF") ;
if(defined $SHOWDIFF) { undef $SHOWDIFF if $SHOWDIFF ne "Y" ; } ;

sub compareFiles($$) {
    my ($a,$b) = @_ ;
    if(!defined $a) {
	return 1 ; # In this context an undef value is higher than any other
    }
    if(!defined $b) {
	return -1 ; 
    }
    if(&P4CGI::IGNORE_CASE() eq "Yes") {
	return uc($a) cmp uc($b) ;
    }
    else {
	return $a cmp $b ;
    }  ;
}

#
# Start page
#
print 
    "",
    &P4CGI::start_page("Diff between label<br> $LABEL1 and $LABEL2","") ;

#
# Get basic data for labels
#
my %label1Data ;
&P4CGI::p4readform("label -o $LABEL1",\%label1Data) ;

my %label2Data ;
my @fields = &P4CGI::p4readform("label -o $LABEL2",\%label2Data) ;

				# Fix View field
{
    $label1Data{"View"} = "<tt>$label1Data{View}</tt>" ; 
    $label2Data{"View"} = "<tt>$label2Data{View}</tt>" ; 
}

#
# Print basic label data
#
print "",			# Start table
    &P4CGI::start_table(""),
    &P4CGI::table_row(-type => "th",
		      "",
		      $LABEL1,
		      $LABEL2) ;

my $f ;
shift @fields ; # remove "Label" from fields (redundant)
				# print label information
foreach $f (@fields) {
    my $f1 = $label1Data{$f} ;
    my $f2 = $label2Data{$f} ;
    if($f1 eq $f2) {
	$f1 = undef ;
    }
    else {
	$f1 = {-align => "center",
	       -bgcolor=>&P4CGI::HDRFTR_BGCOLOR(),
	       -text    =>$f1} ;
    }
    $f2 = {-align => "center",
	   -bgcolor=>&P4CGI::HDRFTR_BGCOLOR(),
	   -text    =>$f2} ;
    print "",&P4CGI::table_row({-align => "right",
				-text  => "<b>$f</b>"},
			       $f1,
			       $f2) ;    
} ;

print "",
    &P4CGI::end_table(),
    "<hr>";
    
#
# Get files for labels
#

				# Get files
my (@filesLabel1,@filesLabel2);
&P4CGI::p4call(\@filesLabel1, "files \"\@$LABEL1\"" );
&P4CGI::p4call(\@filesLabel2, "files \"\@$LABEL2\"" );

				# Remove revision info and create file-to-rev maps
my (%fileRevLabel1,%fileRevLabel2) ;
map { s/\#(\d+).*// ; $fileRevLabel1{$_} = $1 } @filesLabel1 ;
map { s/\#(\d+).*// ; $fileRevLabel2{$_} = $1 } @filesLabel2 ;

				# Sort files
{
    my @tmp = @filesLabel1 ;
    @filesLabel1 = sort { compareFiles($a,$b) ; } @tmp ;
    @tmp =  @filesLabel2 ;
    @filesLabel2 = sort { compareFiles($a,$b) ; } @tmp ;
}
 
				# Get some statistics
my $commonFound=0 ;
my $commonAndSameRev=0 ;
{
    my $f ;
    foreach $f (@filesLabel1) {
	if(exists $fileRevLabel2{$f}) {
	    $commonFound++ ;
	    $commonAndSameRev++ if $fileRevLabel2{$f} == $fileRevLabel1{$f} ;
	}
    }
}

my ($nfiles1,$nfiles2) ;
$nfiles1 = @filesLabel1 ;
$nfiles2 = @filesLabel2 ;
    
my $fileslisted = "Yes" ; 
print "",
    &P4CGI::start_table("COLSPACING=0 CELLPADDING=0"),
    &P4CGI::table_row({-type=>"th",
		       -align=>"right",
		       -text=>"$LABEL1:"}
		      ,"$nfiles1 files"),
    &P4CGI::table_row({-type=>"th",
		       -align=>"right",
		       -text=>"$LABEL2:"}
		      ,"$nfiles2 files"),
    &P4CGI::table_row("","$commonFound common files") ;
if($commonAndSameRev > 0) {
    print "",&P4CGI::table_row("","$commonAndSameRev with same revision") ;
}
print "",
    &P4CGI::end_table() ;    


if($commonFound == 0) {
    print 
	"<FONT SIZE=+2 COLOR=red>",
	"The two labels has no files in common, comparsion aborted.</FONT>" ;
}
else {
    if(defined $SHOWSAME and defined $SHOWNOTSAME and defined $SHOWDIFF) {
	print "<B>Files:</B><br>\n" ;
    }
    elsif(!defined $SHOWSAME and !defined $SHOWNOTSAME and !defined $SHOWDIFF) {
	print "No files listed!<br>\n" ;
	$fileslisted = undef ;
    }
    else {
	print "<B>Listed files are:<BR>\n" ;
	defined $SHOWSAME and do { 
	    print "<LI> Files not modified\n" ; } ;
	defined $SHOWNOTSAME and do { 
	    print "<LI> Modified files (different rev.)\n" ; } ;
	defined $SHOWDIFF and do {
	    print "<LI> Files only in one of the labels $LABEL1 and $LABEL2\n" ; } ;
	print "</B>\n" ;
    } ;
    
#
# Start print list of files
#
    if(defined $fileslisted) {
	print 
	    "",
	    &P4CGI::start_table("border "),
	    &P4CGI::table_row("-type","th",
			      "File",undef,"$LABEL1<br>Rev.",undef,"$LABEL2<br>Rev.") ;
	
	my ($name1,$name2) ;
	while(@filesLabel1 > 0 or @filesLabel2 > 0) { 
	    $name1 = shift @filesLabel1 unless defined $name1 ;
	    $name2 = shift @filesLabel2 unless defined $name2 ;
	    my $rev1 = $fileRevLabel1{$name1} if defined $name1 ;
	    my $rev2 = $fileRevLabel2{$name2} if defined $name2 ;

	    my $cmp = compareFiles($name1,$name2) ;
	    if($cmp == 0) {
		if($rev1 == $rev2 and defined $SHOWSAME) {
		    print &P4CGI::table_row(&P4CGI::ahref("-url",
							      "fileLogView.cgi",
							      "FSPC=$name1",
							      "$name1"),
						undef,undef,undef,
						{-text=>"$rev1",
						 -align=>"center"}) ;
		}
		if($rev1 != $rev2 and defined $SHOWNOTSAME) {		    
		    print &P4CGI::table_row(&P4CGI::ahref("-url","fileLogView.cgi",
							  "FSPC=$name1",
							  "$name1"),
					    {-text=>"$rev1",
					     -align=>"center"},
					    undef,
					    {-text=>&P4CGI::ahref("-url","fileDiffView.cgi",
								  "FSPC=$name1",
								  "REV=$rev1",
								  "REV2=$rev2",
								  "ACT=edit",
								  "<small>diff</small>"),
					     -align=>"center"},
					    {-text=>"$rev2",
					     -align=>"center"}) ;
		}
		$name1 = undef ;
		$name2 = undef ;
		next ;
	    }
	    if($cmp < 0) {
		if(defined $SHOWDIFF) {
		    print &P4CGI::table_row(&P4CGI::ahref(-url => "fileLogView.cgi",
							  "FSPC=$name1",
							  "$name1"),
					    undef,
					    {-text  => "$rev1",
					     -align => "center"},
					    undef,
					    {-text    => "<small>not in label</small>",
					     -align   => "center",
					     -bgcolor => "red"}) ;
		}
		$name1=undef ;
		next ;
	    }
	    else {
		if(defined $SHOWDIFF) {
		    print &P4CGI::table_row(&P4CGI::ahref("-url","fileLogView.cgi",
							  "FSPC=$name2",
							  "$name2"),
					    undef,
					    {-text    => "<small>not in label</small>",
					     -align   => "center",
					     -bgcolor => "red"},
					    undef,
					    {-text    => "$rev2",
					     -align   => "center"}) ;
		}
		$name2=undef ;
		next ;
	    }
	}
	print
	    "",
	    &P4CGI::end_table() ;
    }
}
print &P4CGI::end_page() ;

#
# That's all folks
#




