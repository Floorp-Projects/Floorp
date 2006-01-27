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
#  P4 change viewer
#  View a change by number
#
#################################################################

# Get file spec argument
my $change = P4CGI::cgi()->param("CH") ;
$change = &P4CGI::extract_digits($change);
&P4CGI::bail("No change number specified") unless defined $change ;
&P4CGI::bail("Invalid change number specified") unless ($change =~ /^\d+$/);
$change =~ /^\d+$/ or  &P4CGI::bail("\"$change\" is not a positive number");

my @desc ;
my $currlev = &P4CGI::CURRENT_CHANGE_LEVEL() ;
if($change > $currlev or $change < 1) {
    &P4CGI::signalError("\"$change\" is not a valid change number (0 < change <= $currlev)");
} ;

@desc=&P4CGI::run_cmd("describe", "-s", $change);


$_ = &P4CGI::fixSpecChar(shift @desc) ;

/^$/ and &P4CGI::bail("- no such changelist.");
/no such changelist/ and &P4CGI::bail("- no such changelist.");   
/^Change (\d+) by (\S+)@(\S+) on (\S+) (\d\d:\d\d:\d\d)(.*)$/ or &P4CGI::bail("Illegal syntax on line returned. $_");

my ($chn, $user, $client, $date, $time, $status) = ($1,$2,$3,$4,$5,$6) ;

my $statstr = "" ;
my $pending ;
if ( defined $status and $status =~ /pending/) {
    $statstr="<br>(pending)" ;
    $pending = "yes" ;
} ;

my $legend =&P4CGI::ul_list( "<B>User name</B> -- see user info",
			     "<B>Client name</B> -- see client info",
			     "<B>Filename</B> -- see the complete file history",
			     "<B>Revision Number</B> -- see the file text",
			     "<B>Action</B> -- see the deltas (diffs)") ;
if($pending) {
    $legend =&P4CGI::ul_list( "<B>User name</B> -- see user info",
			      "<B>Client name</B> -- see client info",
			      "<B>Filename</B> -- see the complete file history") ;
}

				# Create title
print "", &P4CGI::start_page("Change $change$statstr",$legend) ;

my $description="" ;
my $leadDescSpace ;
while(@desc > 0) {
    $_ = &P4CGI::fixSpecChar(shift @desc) ;
    chomp ;
    next if /^\s*$/;
    last if /^Jobs fixed/;
    last if /^Affected files/;
    if($_ !~ /^\s*$/) {
	if(defined $leadDescSpace) {
	    s/^$leadDescSpace// ;
	}
	else {
	    s/^(\s+)// ;
	    $leadDescSpace = $1 ;
	};
	$description .= "\n$_";
    }
}

my $jobsFixed="-" ;
if( /^Jobs fixed/ )
{
    $jobsFixed = "\n" ;
    shift @desc ;
    while (@desc > 0) {
	$_ = &P4CGI::fixSpecChar(shift @desc) ;
	my( $job, $time, $user, $client );

	while( ( $job, $time, $user ) = 
	      /(\S*) on (\S*) by (\S*)/ )
	{
	    $jobsFixed .=  &P4CGI::ahref("-url","jobView.cgi",
					 "JOB=$job", 
					 $job) . "\n<br><tt>";
	    shift @desc ;
	    while(@desc > 0){
		$_ = &P4CGI::fixSpecChar(shift @desc) ;
		last if /^\S/;
		$jobsFixed .= $_ . "<br>";
	    }	    
	    $jobsFixed .= "</tt>\n";
	}
	
	last if /^Affected files/;
    }

    $jobsFixed .= "\n" ;
}

my @referenced ;
my $desc = &P4CGI::magic($description,\@referenced) ;
my $referenced="" ;
if(@referenced > 0) {
    my $c ;
    $referenced .= "<dl>\n" ;
    foreach $c (@referenced) {
	my %data ;
	&P4CGI::p4readform("change -o $c",\%data) ;
	if(exists $data{"Description"}) {
	    my $d = &P4CGI::fixSpecChar($data{"Description"}) ;
	    $d =~ s/\n/<br>\n/g ;		
	    $c = &P4CGI::ahref("-url" => "changeView.cgi",
			       "CH=$c",
			       "Change $c") ;
	    $referenced .=  "<dt>$c description:\n<dd><tt>$d</tt>\n" ;
	}
    }
    $referenced .= "</dl>\n" ;
}

print
    "",
    &P4CGI::start_table(""),
    &P4CGI::table_row("-valign","top",{"-type","th", "-align","right", "-text","Author"},
		      &P4CGI::ahref(-url => "userView.cgi",
				    "USER=$user",
				    $user)),
    &P4CGI::table_row("-valign","top",{"-type","th", "-align","right", "-text","Client"},
		      &P4CGI::ahref(-url => "clientView.cgi",
				    "CLIENT=$client",
				    $client)),
    &P4CGI::table_row("-valign","top",{"-type","th", "-align","right", "-text","Date"},
		      "$date $time"),
    &P4CGI::table_row("-valign","top",
		      {"-type","th", "-align","right", "-text","Description"},
		      {"-text","<pre>$desc</pre>"}) ;
print 
    "",
    &P4CGI::table_row(-valign => "top",
		      "",
		      "<small>$referenced</small>") ;
if ( ! defined $pending ) {
    print
	"",
	&P4CGI::table_row("-valign","top",{"-type","th", "-align","right", "-text","Jobs fixed"},
			  "$jobsFixed") ;
} ;
print
    "",
    &P4CGI::end_table();

if(! defined $pending ) {
    print
	"",
	&P4CGI::start_table("cellpadding=1 "),
	&P4CGI::table_header("Action/view diff","Rev/view file","File/file log") ;

# Sample:
# ... //main/p4/Jamrules#71 edit


    my $allfiles ;
    my $allrevs ;
    my $allmodes ;
    my $cnt = 0 ;
    
    while(@desc > 0) {
	$_ = &P4CGI::fixSpecChar(shift @desc) ;
	if(/^\.\.\. (.*)#(\d*) (\S*)$/)  {
	   my( $file, $rev, $act ) = ($1,$2,$3) ;
	   if($act ne "delete") {
	       $cnt++ ;
	       if(defined $allfiles) {
		   $allfiles .= ",$file" ;
		   $allrevs .= " $rev" ;
		   $allmodes .= " $act" ;
	       }
	       else {
		   $allfiles = "$file" ;		   
		   $allrevs = "$rev" ;
		   $allmodes = "$act" ;
	       }
	       print
		   "",
		   &P4CGI::table_row(&P4CGI::ahref("-url","fileDiffView.cgi",
						   "FSPC=$file", 
						   "REV=$rev",
						   "ACT=$act",
						   "$act"),
				     &P4CGI::ahref("-url","fileViewer.cgi",
						   "FSPC=$file", 
						   "REV=$rev","$rev"),
				     &P4CGI::ahref("-url","fileLogView.cgi",
						   "FSPC=$file", "$file")) ;
	   }
	   else {
	       print
		   "",
		   &P4CGI::table_row("$act",
				     "$rev",
				     &P4CGI::ahref("-url","fileLogView.cgi",
						   "FSPC=$file", "$file"));
	   }
       } ;
    } ;
    
    print &P4CGI::end_table(),"<P>" ;
    
    if($cnt > 1) {
	print 
	    "<B>",
	    &P4CGI::ahref("-url","fileDiffView.cgi",
			  "FSPC=$allfiles", 
			  "REV=$allrevs",
			  "ACT=$allmodes",
			  "CH=$change",
			  "View diff for all files in change"),
	    "</B>" ;
	} ;
    
} 
else {
    print 
	"",
	&P4CGI::start_table("cellpadding=1 "),
	&P4CGI::table_header("Action","Rev","File/file log") ;

    my @openfiles ;
    @openfiles=&P4CGI::run_cmd("opened", "-a") ;
    
    my @files ;
    my @revs ;
    my @actions ;
    foreach (@openfiles) {
	if(/(\S+)#(\d+) - (\w+) change $change /) {	   
	   push @files,$1 ;
	   push @revs,$2 ;
	   push @actions,$3 ;
	}
    }

    while(@files > 0) {
	my $file = shift @files ;
#	my $rev = shift @revs ;
	my $act = shift @actions ;
	if($act eq "edit") {
	    print
		"",
		&P4CGI::table_row($act,
				  shift @revs,
				  &P4CGI::ahref("-url","fileLogView.cgi",
						"FSPC=$file", "$file")) ;
	}
	else {
	    print
		"",&P4CGI::table_row($act,
				     shift @revs,
				     $file) ;
	} ;
    } ;
    
    print &P4CGI::end_table(),"<P>" ;

} ;

print  &P4CGI::end_page();

#
# That's all folks
#
