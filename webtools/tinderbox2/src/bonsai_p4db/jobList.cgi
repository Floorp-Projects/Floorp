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
#  P4 change browser
#  View list of jobs
#
#################################################################

#######
# Parameters:
#
#  LIST
#    If defined, show a list, if not, show select dialogue
#
#  JOBVIEW
#    If defined, used as jobview
#
#  FLDnnn
#    These parameters for this script depends on the fileds defined in the
#    jobspec. The parameters are named:
#    FLDnnn
#    Where nnn is the field number as defined in the jobspec
#
#  MATCHTYPE
#    Used with FLDnnn parameters and defines if all or any should match
#
######

###
### Get and parse jobspec
###
my %jobspec ;
&P4CGI::p4readform("jobspec -o",\%jobspec) ;

#
# Make a 2000.2 jobspec compatible with 2000.1 and earlier
#
if(exists $jobspec{"Values"}) {
    foreach (split("\n",$jobspec{"Values"})) {
	my ($fld,$value) = split(/\s+/,$_) ;
	$jobspec{"Values-$fld"} = $value ;
    } ;
}

#
# Get jpbspec fields
#
my %fields ; # Store name, type, len, and options by field number
{
    my @tmp = split("\n",$jobspec{"Fields"}) ;
    my $s ;
    foreach $s (@tmp) {
	my ($code,$name,$type,$len,$option) = split(/\s+/,$s) ;    
	$fields{$code} = [ $name, $type, $len, $option ] ;
    }
}


###
### Build a selection forms for job list
###
sub buildSelection() {
				## Get list of users (for later use for "user" field)
    my @users ;
    &P4CGI::p4call(\@users, "users" );
    my @listOfUsers = sort { uc($a) cmp uc ($b) } map { /^(\S+).*> \((.+)\) .*$/ ; $1 ; } @users ;
    my %userCvt =  map { /^(\S+).*> \((.+)\) .*$/ ; ($1,$2) ; } @users ;
    my $ulistSize = @listOfUsers ;
    $ulistSize= 15 if $ulistSize > 15 ;
    
    my @fieldPrompt ;		# Prompt for each field
    my @field ;			# form entry for each field

    my $code ;			## Loop over all fields (sorted by id)
    foreach $code (sort keys %fields) 
    {
	my ($name,$type,$len,$option) = @{$fields{$code}} ;	
				# Handle "Select" type field
	if($type eq "select") {	
	    my @set = split("/",$jobspec{"Values-$name"}) ;
	    my $size = scalar @set ;
	    if($size > 5) { $size = 5 } ;
	    push @field, &P4CGI::cgi()->scrolling_list(-name      => "FLD".$code,
						       -values    => \@set,
						       -size      => $size,
						       -multiple  => 'true') ;
	    push @fieldPrompt,"$name is one of" ;
	    next ;
	    
	}
				# Date type field
	if($type eq "date") {
	    my %values = ( 
			   1    => "  One Day old",
			   2    => "  Two Days old",
			   3    => "Three Days old",
			   4    => " Four Days old",
			   5    => " Five Days old",
			   6    => "  Six Days old",
			   7    => "  One Week old",
			   7*2  => "  Two Weeks old",
			   7*2  => "Three Weeks old",
			   7*4  => " Four Weeks old",
			   7*5  => " Five Weeks old",
			   7*6  => "  Six Weeks old",
			   7*7  => "Seven Weeks old",
			   7*8  => "Eight Weeks old",
			   7*9  => " Nine Weeks old",
			   7*10 => "   10 Weeks old",
			   7*11 => "   11 Weeks old",
			   7*12 => "   12 Weeks old",
			   7*16 => "   16 Weeks old",
			   7*20 => "   20 Weeks old",
			   7*26 => "   26 Weeks old",
			   7*40 => "   40 Weeks old",
			   7*52 => "   52 Weeks old") ;
	    my @values = sort { $a <=> $b } keys %values ;
	    push @field, join("\n",
			      (&P4CGI::cgi()->popup_menu(-name      => "FLD".$code."cmp",
							 -default   => 0,
							 -values    => ["-",">",">=","<=","<"] ,
							 -labels    => { "-"=>"- Ignore -",
									 ">"=>"Less than",
									 ">="=>"Less than or exactly",
									 "<="=>"More than or exactly",
									 "<"=>"More than" }),
			       &P4CGI::cgi()->popup_menu(-name      => "FLD".$code,
							 -default   => 0,
							 -values    => \@values,
							 -labels    => \%values))
			      ) ;
	    push @fieldPrompt,"$name is" ;
	    next ;
	}	
				# Type must be word, line or text. Compute some lengths for
				# text field
	$len = 256 if $len == 0 ;
	my $displen = $len ;
	$displen = 40 if $displen > 40 ;
	my $textfield =  &P4CGI::cgi()->textfield(-name      => "FLD".$code,
						  -size      => $displen,
						  -maxlength => $len) ;	
				# Field type word
	if($type eq "word") {
	    if($code == 101) {
				# Reserved field Job
		push @fieldPrompt,"Job name is" ;
		push @field, $textfield ;
		next ;		
	    }
	    else {
		if($code == 103) {
				# Rserved field User
		    push @fieldPrompt,"User is one of" ;
		    push @field, &P4CGI::cgi()->scrolling_list(-name      => "FLD$code",
							       -values    => \@listOfUsers,
							       -size      => $ulistSize,
							       -multiple  => 'true',
							       -labels    => \%userCvt) ;
		    next ;
		}
		push @fieldPrompt,"$name is" ;
		push @field, $textfield ;
		next ;		
	    }
	}
				# Field type line or text
	if($type eq "line" or $type eq "text") {
	    push @fieldPrompt,"$name contains one of the words" ; 	
	    push @field, $textfield ;
	    next ;			    
	}
    } # end loop over fields

				# Add field for match for "any" or "all" fields
    push @fieldPrompt,"Select type of match" ;    
    push @field, &P4CGI::cgi()->popup_menu(-name      => "MATCHTYPE",
					   -default   => 0,
					   -values    => ["all","any"] ,
					   -labels    => { "all"=>"Match all fields above",
							   "any" =>"Match any field above"}) ; # 
    				# Create table contents from fields
    my @tmp ;
    while(@field > 0) {
	my $pr = shift @fieldPrompt ;
	my $fld =  shift @field ;
	push @tmp,("<tr>",
		   "<td align=right valign=center>",$pr,":</td>",
		   "<td align=left valign=center><font face=fixed>",
		   $fld,
		   "</font></td></tr>") ;
    } ;
				# Return table and form
    return
	join("\n",
	     (&P4CGI::start_table("bgcolor=".&P4CGI::HDRFTR_BGCOLOR().
				  " align=center cellpadding=0 cellspacing=2"),
	      "<tr><td>\n",
	      &P4CGI::start_table("width=100% cellspacing=4"),
	      &P4CGI::table_row(-valign=>"center",
				{-align=>"center",
				 -text => 
				     join("\n",
					  &P4CGI::cgi()->startform(-action => "jobList.cgi",
								   -method => "GET"),
					  "<font size=+1>Select jobs</font>")},
				{-align=>"left",
				 -valign=>"top",
				 -text => join("\n",(&P4CGI::start_table(),
						     @tmp,
						     "</table>"))},
				{-align=>"left",
				 -text => " "},
				{-align=>"left",
				 -valign=>"bottom",
				 -width=>"1",
				 -text =>  &P4CGI::cgi()->submit(-name  => "LIST",
								 -value => "List Jobs")
				 },
				{ -valign=>"bottom",
				  -text => &P4CGI::cgi()->endform()
				  },
				),
    	      &P4CGI::end_table(),
	      "</tr></td>",
	      &P4CGI::end_table())) ;
} # end buildSelection()

unless(defined &P4CGI::cgi()->param("LIST"))
{
    my $selection = &buildSelection() ;
    my @legend ;
    push @legend,&P4CGI::ahref("LIST=Y",
			       "MATCHTYPE=",
			       "List all jobs") ;		
    if(exists $fields{"102"}) { # Check that we have a status field (code 102)
	my $name = $ { $fields{"102"}}[0] ;
	if(exists $jobspec{"Values-$name"}) { # Check that we have the values
	    my @values = split('/',$jobspec{"Values-$name"}) ;
	    my $v ;
	    foreach $v (@values) {
		push @legend,&P4CGI::ahref("FLD102=$v",
					   "LIST=Y",
					   "MATCHTYPE=",
					   "List jobs with $name=$v") ;		
	    }
	}    	
    }
    
    print 
	"", 
	&P4CGI::start_page("View job list",&P4CGI::ul_list(@legend)),
	$selection ;
} 
else {
				# Do we have "JOBVIEW"?
    my $jobview =  &P4CGI::cgi()->param("JOBVIEW") ;
    $jobview = "Yes" if defined $jobview;
    my $jobviewDesc ;
    if(defined $jobview) {
	$jobviewDesc = "Where jobview is: <TT>$jobview</TT>" ;
    }
				# If not, build a job view
    if(! defined $jobview) {
	$jobview = "" ;
	$jobviewDesc="" ;
				# Get field parameters
	my @selectParams = grep { /^FLD/ ; } P4CGI::cgi()->param ;
	my %params ;
	foreach (@selectParams) {
            &P4CGI::bail("Invalid field parameter.") if (/[<>"&:;'`]/);
	    my $v = $_ ;
	    s/^FLD// ;
	    my @pars = &P4CGI::cgi()->param($v) ; 
	    $params{$_} = \@pars ;	
	}
				# Set match all/any
	my $MATCHTYPE = &P4CGI::cgi()->param("MATCHTYPE") ;
	$MATCHTYPE="all" unless defined $MATCHTYPE and $MATCHTYPE eq "any";
	my $matchtype = "|" ;
	my $matchtypeDesc = "or" ;
	if($MATCHTYPE eq "all") {
	    $matchtype = "" ;
	    $matchtypeDesc = "and" ;
	} ;
				# Loop over field parameters
	my $id ;
	foreach $id (grep {/^\d+$/} keys %params) {
	    my $desc ;
	    next unless exists $fields{$id} ;
	    my ($name,$type,$len,$option) = @{$fields{$id}} ;
	    my @p = @{ $params{$id}} ;
	    if($type eq "text" or
	       $type eq "line") {
		my @tmp = map { split ; } @p ;
		@p = @tmp ;
	    }  ;
	    if(@p > 0 and length($p[0]) > 0) {
		my $thisItem ;
		if($type eq "date") {
		    my @cmp = @{ $params{"${id}cmp"}} ;
		    my $cmp = shift @cmp ;
		    next if $cmp eq "-" ;
		    my $time = time()-(24*3600*$p[0]) ;
		    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($time);
		    $year = 1900+$year ;
		    $thisItem = sprintf("$name$cmp$year/%02.2d/%02.2d",$mon+1,$mday) ;
		    $desc .= "$thisItem" ;
		}
		else {	 
		    if(@p == 1) {
			$thisItem = "$name=$p[0]" ;
			$desc .= "$name is \"$p[0]\"" if $type eq "select" ;
			$desc .= "$name contains \"$p[0]\"" 
			    if $type =~ /^(word|line|text)/ ;
		    }
		    else {
			$thisItem = "($name=" . join("|$name=",@p) . ")" ;
			$desc .= "$name is"  if $type eq "select" ;
			$desc .= "$name contains" if $type =~ /^(word|line|text)/ ;
			$desc .= " one of" if @p > 2 ;			
			my $last = pop @p ;
			$desc .=  " \"". join('","',@p) . "\" or \"$last\""  ;
		    }
		}
		if(length($jobview) > 0) {
		    $jobview .= " $matchtype $thisItem" ;
		    $jobviewDesc .= "<br><font>$matchtypeDesc</font><br>\n$desc" ; 
		}
		else {
		    $jobview = "$thisItem" ; 
		    $jobviewDesc = $desc ;
		}
	    }	
	} ;
    } ;
    &P4CGI::cgi()->delete("LIST") ;
    my $legend = &P4CGI::ul_list("<b>Job name</b> -- see details of job",
				 "<b>User</b> -- Information about user",
				 &P4CGI::ahref("-url" => &P4CGI::cgi()->self_url(),
					       "New selection")) ;


    print  "",  &P4CGI::start_page("View job list<br>$jobviewDesc",$legend) ;
    &P4CGI::ERRLOG("jobView:\"$jobview\"") ;
    my @tmp ;
    $jobview = "-e \"$jobview\"" if length($jobview) > 0 ;
    &P4CGI::p4call(\@tmp, "jobs -l $jobview" );
    if(@tmp == 0) {
	print
	    "<font color=red size=+1>No matching jobs found for:<br><tt>jobs -l $jobview</tt></font><hr>",
	    &buildSelection() ;
    }
    else {
	print "<dl>\n" ;
	while (@tmp > 0) {
	    my $l = shift @tmp ;
	    $l =~ /^(\S+) (on \S+ by) (\S+) (.*)/ and do {
		my ($job,$date,$user,$status) = ($1,$2,$3,$4) ;
		$status =~ s/\*(\S+)\*/Status: <tt>$1<\/tt>/ ;
		my $SPACESTR = "          " ;
		my $xsp = "" ;
		if(length($job) < length($SPACESTR)) {
		    $xsp = "<tt>" . substr($SPACESTR,length($job)) . "</tt>" ;
		    $xsp =~ s/ /&nbsp;/g ;
		} ;
		
		print 
		    "<dt>",
		    &P4CGI::ahref("-url" => "jobView.cgi",
				  "JOB=$job",
				  "<tt>$job</tt>"),		    
		    "$xsp $date ",
		    &P4CGI::ahref("-url" => "userView.cgi",
				  "USER=$user",
				  $user),
		    " $status\n" ;
		shift @tmp ;
		print "<dd><pre>" ;
		while(@tmp > 0) {
		    my $desc = shift @tmp ;
		    $desc =~ s/^\s+// ;
		    last if length($desc) == 0 ;
		    print "$desc\n" ;
		}
		print "</pre>" ;
	    }
	} 
	print "</dl>\n" ;
	print "<pre>    ",join("\n    ",@tmp),"</pre>" ;
    }

} ;  

print &P4CGI::end_page() ;        
    
#
# That's all folks
#




