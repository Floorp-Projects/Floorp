#!/usr/bin/perl -w
# -*- perl -*-
use P4CGI ;
use strict ;
#
#################################################################
#  CONFIGURATION INFORMATION 
#  All config info should be in the config file
#
#################################################################
#
#  P4 view job
#  View a job
#
#################################################################

# Get file spec argument
my $job = P4CGI::cgi()->param("JOB") ;
&P4CGI::bail("No job specified") unless defined $job ;
&P4CGI::bail("Invalid job.") if ($job =~ /[<>"&:;'`]/);

				# Create title
print "", &P4CGI::start_page("Job $job","") ;

my @fields ;
my %fieldData ;
@fields = &P4CGI::p4readform("job -o $job",\%fieldData);

				# Check that job exist
if($fieldData{"Description"} =~ /<enter description here>/) {
    &P4CGI::signalError("Job $job does not exist") ;
}
				# Fix user field
if(exists $fieldData{"User"}) {
    $fieldData{"User"} =  &P4CGI::ahref(-url => "userView.cgi",
					"USER=$fieldData{User}",
					$fieldData{"User"}) ;
}
				# Fix description field
if(exists $fieldData{"Description"}) {
    my $d =  &P4CGI::fixSpecChar($fieldData{"Description"}) ;
    $d =~ s/\n/<br>/g ;
    $fieldData{"Description"} = "<tt>$d</tt>" ;
}

my @fixes ;
&P4CGI::p4call(\@fixes,"fixes -j $job") ;

if(@fixes > 0) {
    push @fields,"Fixed by" ;
    $fieldData{"Fixed by"} = join("<br>\n",
				  map {/change (\d+) on (\S+) by (\S+)\@(\S+)/ ;
				       my ($ch,$date,$user,$client) = ($1,$2,$3,$4) ;
				       $ch = &P4CGI::ahref(-url => "changeView.cgi",
							   "CH=$ch",
							   $ch) ;
				       $user = &P4CGI::ahref(-url => "userView.cgi",
							     "USER=$user",
							     $user) ;
				       $client = &P4CGI::ahref(-url => "clientView.cgi",
							       "CLIENT=$client",
							       $client) ;
				       "Change $ch on $date by $user\@$client" ; } @fixes ) ;    
}



print
    "",
    &P4CGI::start_table("") ;

my $f ;
foreach $f (@fields) {
    print &P4CGI::table_row({-align => "right",
			     -valign => "top",
			     -type  => "th",
			     -text  => $f},
			    $fieldData{$f}) ;
} ;

print &P4CGI::end_table("") ;

print  &P4CGI::end_page();

#
# That's all folks
#









