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
#  View a branch
#
#################################################################


###
### Get branch name
###
my $branch = P4CGI::cgi()->param("BRANCH") ;
$branch = &P4CGI::extract_printable_chars($branch);
&P4CGI::bail("No branch specified") unless defined $branch ;
&P4CGI::bail("Invalid branch specified") if $branch =~ /[<>"&:;'`]/;


###
### Get info about branch
###
my %values ;
my @fields = &P4CGI::p4readform("branch -o $branch",\%values) ;

				# Get real user names...
my %userCvt ;
{
    my @users ;
    @users=&P4CGI::run_cmd("users" );
    %userCvt =  map { /^(\S+).*> \((.+)\) .*$/ ; ($1,$2) ; } @users ;
}
				# Fix owner field
if (exists $values{"Owner"}) {
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

				# Fix description field
if(exists $values{"Description"}) {
    my $d = $values{"Description"} ;
    $values{"Description"} = "<pre>$d</pre>" ;
}

				# Fix up view info
my $viewFrom ="";
my $viewTo ="";
my $allfrom ="";
my $allto ="";
foreach (split("\n",$values{"View"})) {
    /^\s*\/\/(.+)\s+\/\/(.+)/ ;
    my ($from,$to) = ($1,$2) ;
    $allfrom .= "//$from" ;
    $allto .= "+//$to" ;
    my @from = split /\//,$from ;
    my @to = split /\//,$to ;
    my $common = "//" ;
    while(@from != 0 and @to != 0  and ($from[0] eq $to[0])) {
	$common .= shift @from ;
	$common .= "/" ;
	shift @to ;
    }
    $from = $common . "<FONT COLOR=red>" . join("/",@from) . "</FONT>" ;
    $to = $common . "<FONT COLOR=red>" . join("/",@to) . "</FONT>" ;
    if (length($viewFrom) > 0) {
	$viewFrom .= "<br>" ;
	$viewTo .= "<br>" ;
    }
    $viewFrom .= "<tt>$from&nbsp;</tt>" ;
    $viewTo   .= "<tt>$to</tt>" ;
} ;
$allto =~ s/^\+// ;

$values{"View"} = join("\n",(&P4CGI::start_table("cellspacing=0 cellpadding=0"),
			     &P4CGI::table_row($viewFrom,$viewTo),
			     &P4CGI::end_table())) ;


my $allToURL = &P4CGI::ahref(-url => "changeList.cgi",
			     "FSPC=$allto",
			     "List changes in branch") ;
my $recentlyChanged = &P4CGI::ahref(-url => "filesChangedSince.cgi",
				    "FSPC=$allto",
				    "List recently changed files in branch") ;
my $listByUser = &P4CGI::ahref(-url => "changeByUsers.cgi",
			       "FSPC=$allto",
			       "List changes in branch for selected user") ;
my $depotStats = &P4CGI::ahref(-url => "depotStats.cgi",
			       "FSPC=$allto",
			       "View depot statistics for branch") ;
my $allFromURL = &P4CGI::ahref(-url => "changeList.cgi",
			       "FSPC=$allfrom",
			       "View changes in branch source") ;
my $searchDesc = &P4CGI::ahref(-url => "searchPattern.cgi",
			       "FSPC=$allto",
			       "Search for pattern in change descriptions") ;
my $openFiles = &P4CGI::ahref(-url => "fileOpen.cgi",
			       "FSPC=$allto",
			       "List open files in branch") ;

###
### Print html
###
print "",
    &P4CGI::start_page("Branch $branch",
		       &P4CGI::ul_list("<b>owner</b> -- view user info",
				       $allFromURL)) ;


print  &P4CGI::start_table("") ;

my $f ;
foreach $f (@fields) {
    print &P4CGI::table_row({-align => "right",
			     -valign => "top",
			     -type  => "th",
			     -text  => "$f"},
			    $values{$f}) ;
} ;

print 
    &P4CGI::end_table(),
    "<hr>";

my @labels ;
&P4CGI::p4call(*P4, "labels" );
while(<P4>) {
    chomp ;
    /^Label\s+(\S+)\s/ and do { push @labels,$1 ; } ;
}
close P4 ;


my $chnotinlabel= join("\n",(&P4CGI::cgi()->startform(-action => "changeList.cgi",
						      -method => "GET"),
			     &P4CGI::cgi()->hidden(-name=>"FSPC",
						   -value=>"$allto"),
			     "View changes not in label:<font size=+0>",	
			     &P4CGI::cgi()->popup_menu(-name  => "EXLABEL",
						       -value => \@labels),
			     &P4CGI::cgi()->submit(-name  => "Go",
						   -value => "Go"),
			     "</font>",			     
			     &P4CGI::cgi()->endform())) ;


print "<font size=+1>" , &P4CGI::ul_list($allToURL,
					 $chnotinlabel,
					 $listByUser,
					 $recentlyChanged,
					 $openFiles,
					 $searchDesc,
					 $depotStats) , "</font>" ;


print
    &P4CGI::end_page() ;

#
# That's all folks
#
