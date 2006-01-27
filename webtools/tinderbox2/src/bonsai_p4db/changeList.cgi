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
#  P4 change browser
#  View list of changes for selected part of depot
#
#################################################################

#####
#   This is (or was) the most complicated and insane script in P4DB.
#   The reason for this is that features where added and added and added until
#   the script became impossible to maintain and no more features could
#   be added. Despite this I still wanted MORE FEATURES and finally I
#   decided to start all over again and see if it is possible to add the
#   new features if I rewrote it all. And maybe, just maybe, I will
#   manage to make it maintainable this time. We will see......
#                                                    Jan 7 - 2000/Fredric
####

#######
# Arguments:
#
# FSPC
#    File specification. Should start with //. Can be more than one file spec
#    in which case they are concatenated (no space, but start each file spec
#    with //).
#    The file specification should not contain any label or revision numbers.
#
# LABEL
#    Label specification. Label for which changes should be listed.
#    There can be only one label.
#
#    NOTE! FSPC or LABEL must be specified.
#
# STATUS
#    Status of changes. Allowed values are "submitted" and "pending".
#    If not supplied "submitted" is assumed. 
#
# EXLABEL (optional)
#    Specify a label to exclude. Same format as LABEL. The changes made for
#    this label are excluded from list. 
#
# MAXCH (optional)
#    Max changes to display. To avoid very large html pages this argument
#    limits the number of changes displayed at one page.
#    If not specified there is a default value used (from config file)
#
# FIRSTCH (optional)
#    When MAXCH is specified this specifies change to start at (for second
#    and subsequent pages)
#
# CHOFFSET (optional)
#    Number of changes already displayed.
#
# SEARCHDESC (optional)
#    View only changes with pattern in description
#
# SEARCH_INVERT (optional)
#    If specified, invert pattern search
#
# USER (optional)
#    View only changes for users specified in comma-separated list
#
# GROUP (optional)
#    View only changes for users in comma-separated list of groups
#
# CLIENT (optional)
#    View only changes for clients specified in comma-separated list 
# 
# SHOWREFERENCED (optional)
#    If present and set to "Y" will try to display description for changes
#    referred to in change description
#    
######


my $MAGIC_RED=":::RED:::~~~" ;

###                         ###
###  Get command arguments  ###
###                         ###

#
# Get file spec argument
#
my $filespec = P4CGI::cgi()->param("FSPC") ;
$filespec = &P4CGI::extract_printable_chars($filespec) if ($filespec);
my $FSPC_WasSpecified = $filespec ;
$filespec = "//..." unless $filespec ;
$filespec =~ s/\s*\+\s*\/\//\/\//g ; # replace <space>+<space>// with //
				     # where <space> is 0 or more whitespace charcaters  
                                     
&P4CGI::bail("Invalid file spec.") if ($filespec =~ /[<>"&:;'`]/);

my @FSPC = 
    map { 
	if($_) {  "//".$_ ; }
	else   {       () ; } ; 
    } split("//", $filespec ) if defined $filespec ; 

#
# Get label argument
#
my $LABEL = P4CGI::cgi()->param("LABEL") ;
$LABEL = &P4CGI::extract_printable_chars($LABEL) if ($LABEL);
if(defined $LABEL and $LABEL eq "-") { $LABEL = undef ; } ;
&P4CGI::bail("Invalid label.") if ($LABEL) && ($LABEL =~ /[<>"&:;'`]/);

#
# Check that FSPC or LABEL is specified
#
unless(defined $LABEL or defined $FSPC_WasSpecified) {
    &P4CGI::bail("File spec OR label must be specified") ; 
}

#
# Get label to exclude
#
my $EXLABEL = &P4CGI::cgi()->param("EXLABEL") ;
if(defined $EXLABEL and $EXLABEL eq "-") { $EXLABEL = undef ; } ;
&P4CGI::bail("Invalid label to exclude: '$EXLABEL'.") if ($EXLABEL) && ($EXLABEL =~ /[<>"&:;'`]/);

#
# Get status
#
my $STATUS = &P4CGI::cgi()->param("STATUS") ;
unless(defined $STATUS) { $STATUS = "submitted" ; } ;
unless ($STATUS =~ /^\w+$/) { &P4CGI::bail("Invalid status: '$STATUS'."); };

#
# Get max changes to display
#
my $MAXCH = P4CGI::cgi()->param("MAXCH") ;
$MAXCH = &P4CGI::extract_digits($MAXCH) if ($MAXCH);
$MAXCH = &P4CGI::MAX_CHANGES() unless($MAXCH) ;

#
# Get first change No. to display and offset from start
#
my $FIRSTCH ;
my $CHOFFSET=0  ;
if(defined $MAXCH) {
    $FIRSTCH = P4CGI::cgi()->param("FIRSTCH") ;
    $FIRSTCH = P4CGI::extract_digits($FIRSTCH) if ($FIRSTCH);
    $CHOFFSET = P4CGI::cgi()->param("CHOFFSETDISP") ;
    $CHOFFSET = P4CGI::extract_digits( $CHOFFSET) if ($CHOFFSET);
}


my $SHOWREFERENCED = P4CGI::cgi()->param("SHOWREFERENCED") ;
$SHOWREFERENCED = undef if defined $SHOWREFERENCED and $SHOWREFERENCED ne "Y" ;

#
# Get search data, user and client parameters
#
my $SEARCHDESC = &P4CGI::cgi()->param("SEARCHDESC") ;
$SEARCHDESC= &P4CGI::extract_printable_chars($SEARCHDESC) if ($SEARCHDESC);
$SEARCHDESC =~ s/^<pattern>$//;
$SEARCHDESC=undef if defined $SEARCHDESC and $SEARCHDESC eq "" ;
&P4CGI::bail("Invalid search terms: '$SEARCHDESC'.") if ($SEARCHDESC) && ($SEARCHDESC =~ /[<>"&:;'`]/);

my $SEARCH_INVERT = &P4CGI::cgi()->param("SEARCH_INVERT") ;
&P4CGI::bail("Invalid search terms: '$SEARCH_INVERT'.") if ($SEARCH_INVERT) && ($SEARCH_INVERT =~ /[<>"&:;'`]/);

my $USER = &P4CGI::cgi()->param("USER") ;
$USER = &P4CGI::extract_user($USER) if ($USER);
{
    my @tmp = map { &P4CGI::extract_user($_) } &P4CGI::cgi()->param("USERS") ;
    if(@tmp) {
	if(defined $USER) {
	    $USER .= "," . join(',',@tmp) ;
	} 
	else {
	    $USER = join(',',@tmp) ;
	}
    }
}
$USER=undef if defined $USER and $USER eq "" ;
&P4CGI::bail("Invalid user(s).") 
    unless (!defined($USER) || ($USER =~ /^\w+(,\w+)*$/));

my $GROUP = &P4CGI::cgi()->param("GROUP") ;
$GROUP = &P4CGI::extract_user($GROUP) if ($GROUP);
{
    my @tmp = map { &P4CGI::extract_user($_) } &P4CGI::cgi()->param("GROUPS") ;
    if(@tmp) {
	if(defined $GROUP) {
	    $GROUP .= "," . join(',',@tmp) ;
	} 
	else {
	    $GROUP = join(',',@tmp) ;
	}
    }
}
$GROUP=undef if defined $GROUP and $GROUP eq "" ;
&P4CGI::bail("Invalid group(s).") if ($GROUP =~ /[<>"&:;'`]/);

my $CLIENT = &P4CGI::cgi()->param("CLIENT") ;
$CLIENT = &P4CGI::extract_printable_chars($CLIENT) if ($CLIENT);
$CLIENT=undef if defined $CLIENT and $CLIENT eq "" ;
&P4CGI::bail("Invalid client specified.") if ($CLIENT) && ($CLIENT =~ /[<>"&:;'`]/);

my $WEEKS = P4CGI::cgi()->param("WEEKS") ;
if(defined $WEEKS) {
    $WEEKS = P4CGI::extract_digits($WEEKS) ;
} 
else {
    $WEEKS = 0 ;
}

my $DAYS = P4CGI::cgi()->param("DAYS");
if(defined $DAYS) {
    $DAYS = P4CGI::extract_digits($DAYS);
} 
else {
    $DAYS=0 ;
}

my $HOURS = P4CGI::cgi()->param("HOURS");
if(defined $HOURS) {
    $HOURS = P4CGI::extract_digits($HOURS);
} 
else {
    $HOURS = 0 ;
}
my $SUBMIT = P4CGI::cgi()->param("SUBMIT");
if(defined $SUBMIT) {
    $SUBMIT=1;
} else {
    $SUBMIT='';
}
    
my $MINDATE = P4CGI::cgi()->param("MINDATE") ;
$MINDATE = &P4CGI::extract_printable_chars($MINDATE);

my $MAXDATE = P4CGI::cgi()->param("MAXDATE") ;
$MAXDATE = &P4CGI::extract_printable_chars($MAXDATE);
 
my $DATESPECIFIER = P4CGI::cgi()->param("DATESPECIFIER") ;
$DATESPECIFIER = &P4CGI::extract_printable_chars($DATESPECIFIER);

my $TIMEINTERVALSTR;

if      ($DATESPECIFIER eq 'explicit') {
     $TIMEINTERVALSTR = "\@".&P4CGI::DateStr2Time($MINDATE).",\@".&P4CGI::DateStr2Time($MAXDATE);
} elsif ($DATESPECIFIER eq 'picklist') {
     $TIMEINTERVALSTR = &P4CGI::DateList2Time($WEEKS, $DAYS, $HOURS);
} else {
     $TIMEINTERVALSTR = '';
}

my $SHOWFILES = P4CGI::cgi()->param("SHOWFILES") ;
if(defined $SHOWFILES) {
    $SHOWFILES='Y';
} else {
    undef $SHOWFILES;
;
}

my %allUsers;



###
### Sub getChanges
###
# This subroutine is used to get a set of changes from depot. The parameter is a hash containing
# a set of switches:
#   -long     If set, get changes in "long" format (i.e. full description,
#             not only the first 27 chars)
#   -file     File spec for changes
#   -firstch  First change to look for (or "offset")
#   -maxch    Max changes to get
#   -status   Status ("submitted" or "pending")
#   -label    Label for file spec
#   -lastch   Reference to scalar to receive last change parsed
#   -lastrch  Reference to scalar to receive last change parsed and returned in result
#   -resultto A reference to a hash to receive result
#   -select   A reference to a subroutine to call that determine if a change should be included
#             in list. The subroutine gets parameters: (<change>,<date>,<user>,<client>,<description>)
#             The <description> parameter is passed as a reference and can be modified
#             The subroutine should return true if the change should be included.
#             NOTE! The -select parameter is very important to understand if you plan to
#                   understand more of this code.
#
# Another important thing to understand is that this subroutine getChanges is frequently called 
# more than once.
# 
sub getChanges(%)
{
    my %pars = @_ ;
    my $long ;			# defined if -l flag
    my $filespec = "" ;		# file spec
    my $firstch = &P4CGI::CURRENT_CHANGE_LEVEL() ; # first change to look for
    my $maxch = 0 ;		# max no changes
    my $status="submitted" ;	# status
    my $label ;			# label
    my $rhash ;			# result
    my $select ;		# selection funtion
    my $lastch ;		# Last change parsed
    my $lastrch ;		# Last change parsed and returned
    my $linkedch ;              # linked ch ref
    my $k ;
    foreach $k (keys %pars) {
	$k = lc($k) ;
	$k eq "-long"        and do { $long      = $pars{$k} ; next } ;
	$k eq "-file"        and do { $filespec  = $pars{$k} ; next } ;
	$k eq "-firstch"     and do { $firstch   = $pars{$k} ; next } ;
	$k eq "-maxch"       and do { $maxch     = $pars{$k} ; next } ;
	$k eq "-status"      and do { $status    = $pars{$k} ; next } ;
	$k eq "-label"       and do { $label     = $pars{$k} ; next } ;
	$k eq "-resultto"    and do { $rhash     = $pars{$k} ; next } ;
	$k eq "-select"      and do { $select    = $pars{$k} ; next } ;
	$k eq "-lastch"      and do { $lastch    = $pars{$k} ; next } ;
	$k eq "-lastrch"     and do { $lastrch   = $pars{$k} ; next } ;
	$k eq "-magiclinked" and do { $linkedch  = $pars{$k} ; next } ;
    } ;
    my $tmpLastch ;
    $lastch = \$tmpLastch unless defined $lastch ;
    $lastrch = \$tmpLastch unless defined $lastrch ;
    if($long) { $long = " -l " ; } else { $long = "" } ;
    
    if ($TIMEINTERVALSTR) {
	$filespec .= $TIMEINTERVALSTR;
    } elsif($label) {
	$filespec .= "\@$label" ;
    } else {
	if ($firstch) {
	    $filespec .= "\@$firstch" ;
	} ;
    } ;
    if($maxch) { $maxch = " -m $maxch " ;} else { $maxch = "" ; } ;
    if($status eq "pending") {
	$filespec = "" ;
    } ;
    if($filespec =~ /\s/) {
	$filespec = "\"$filespec\"" ;
    } ;

    my $command = "changes $long -s $status $maxch $filespec" ;
    local *P4 ;
    my $n = 0 ;
    &P4CGI::p4call(*P4,$command) ;
    $$lastrch = 0 ;
    while(<P4>) {
  chomp ;
	if(/Change (\d+) on (\S+) by (\w+)\@(\S+)/) {
      $n++ ;
	    my ($change,$date,$user,$client) = ($1,$2,$3,$4) ;
	    $$lastch = $change ;
	    my $desc = "" ;
	    if($long) {
		<P4> ;
		while(<P4>) {
		    chomp ;
		    last if length($_) == 0 ; 
		    s/^\t// ;
		    if(length($desc) > 0) {	$desc .="\n" ; } ;
		    $desc .= $_ ;
		}
#		&P4CGI::ERRLOG("select: $select") ;
		if(defined $select) {
		    next unless &$select($change,$date,$user,$client,\$desc) ;
		}
		my @ch; 
		$desc = "<pre>" . &P4CGI::magic(&P4CGI::fixSpecChar($desc),\@ch) . "</pre>\n" ;
		$desc =~ s/$MAGIC_RED(.*?)$MAGIC_RED/<font color=red>$1<\/font>/gi ;
		if(defined $linkedch and @ch > 0) {
		    $$linkedch{$change} = \@ch ;
		} ;
	    } ;
	    $$lastrch = $change ;
	    $$rhash{$change} = {'date'=>$date,'user'=>$user,'client'=>$client,'desc'=>$desc} ;
	}
    }    
    close P4 ; 
    return $n ;
} ;




sub getDescription 
{
    my ($change, $rhash) = @_ ;

    my $command = "describe -s $change" ;
    local *P4 ;
    my $n = 0 ;
    &P4CGI::p4call(*P4,$command) ;
    while(<P4>) {
	chomp ;
	if(/Change (\d+) by (\w+)\@(\S+) on (\S+) (\S+)/) {
	    $n++ ;
	    my ($change,$user,$client,$date,$time) = ($1,$2,$3,$4,$5) ;
	    my @files =();
	    my $desc = "" ;
	    <P4> ;
	    while(<P4>) {
		chomp ;
		last if length($_) == 0 ; 
		s/^\t// ;
		if(length($desc) > 0) {	$desc .="\n" ; } ;
		$desc .= $_ ;
	    }
 	    <P4> ;
 	    <P4> ;
	    while(<P4>) {
		chomp ;
		if (m/... (\S+)\#(\d+)/) {
		    my ($file, $rev) = ($1, $2);
		    push @files, (  
				    &P4CGI::ahref("-url","fileLogView.cgi",
						  "FSPC=$file", "$file"),
				    "\#",
				    &P4CGI::ahref("-url","fileViewer.cgi",
						  "FSPC=$file", 
						  "REV=$rev","$rev"),
            &P4CGI::ahref("-url","fileDiffView.cgi",
              "FSPC=$file",
              "REV=$rev",
              "ACT=edit",
              "edit"),
				    "<br>\n"
				    );
		}
	    }
	    
	    my @ch; 
	    $desc = "<pre>" . &P4CGI::magic(&P4CGI::fixSpecChar($desc),\@ch) . "</pre>\n" ;
	    $desc =~ s/$MAGIC_RED(.*?)$MAGIC_RED/<font color=red>$1<\/font>/gi ;
	    $$rhash{$change} = {
		'date'=>$date,'time'=>$time,
		'user'=>$user,'client'=>$client,
		'desc'=>$desc, 'files'=>[@files],
		} ;

	}
    }    
    close P4 ; 
    return $n ;
} ;


sub get_params {
    my @params=''; 
    @params = ("STATUS=$STATUS",
	       "MAXCH=$MAXCH") ;
    if(defined $EXLABEL) {
	push @params,"EXLABEL=$EXLABEL" ;
    } ;
    if(defined $LABEL) {
	push @params,"LABEL=$LABEL" ;
    } ;
    if(defined $USER) {
	push @params,"USER=$USER" ;
    } ;
    if(defined $GROUP) {
	push @params,"GROUP=$GROUP" ;
    } ;
    if(defined $SEARCHDESC) {
	push @params,"SEARCHDESC=$SEARCHDESC" ;
    } ;
    if(defined $SEARCH_INVERT) {
	push @params,"SEARCH_INVERT=1" ;
    } ;
    if(defined $SHOWREFERENCED) {
	push @params,"SHOWREFERENCED=$SHOWREFERENCED" ;
    } ;
    if(defined $WEEKS) {
	push @params,"WEEKS=$WEEKS" ;
    } ;
    if(defined $DAYS) {
	push @params,"DAYS=$DAYS" ;
    } ;
    if(defined $HOURS) {
	push @params,"HOURS=$HOURS" ;
    } ;
    if(defined $MINDATE) {
	push @params,"MINDATE=$MINDATE" ;
    } ;
    if(defined $MAXDATE) {
	push @params,"MAXDATE=$MAXDATE" ;
    } ;
    if(defined $DATESPECIFIER) {
	push @params,"DATESPECIFIER=$DATESPECIFIER" ;
    } ;
    if(defined $SHOWFILES) {
	push @params,"SHOWFILES=$SHOWFILES" ;
    } ;
    if($FSPC_WasSpecified) {
	push @params,"FSPC=".P4CGI::cgi()->param("FSPC") ;
    }
    
    return @params;
}




### get target ###
my %extraUrlOptions ;
if(&P4CGI::CHANGES_IN_SEPPARATE_WIN()) {
    $extraUrlOptions{"-target"}="CHANGES" ;
}
###                ###
### Fix page title ###
###                ###

my $title = "Changes for " ;
if($FSPC_WasSpecified) {
    $title .= "<br><tt>" . join("<br>",@FSPC) . "</tt>" ;
    if(defined $LABEL) {
	$title .= "<br>and label <tt>$LABEL</tt>" ;
    }
}
else {
    $title .= "label <tt>$LABEL</tt>" ;
} ;
if(defined $EXLABEL) {
    $title .= "<br>excluding changes for label <tt>$EXLABEL</tt>" ;
}        
if(defined $CHOFFSET and $CHOFFSET > 0) {
    $title .= "<br><small>(offset $CHOFFSET from top)</small>" ;
} ;
if(defined $USER) {
    $title .= "<br>user: <tt>$USER</tt>" ;
} ;
if(defined $GROUP) {
    $title .= "<br>group: <tt>$GROUP</tt>" ; # 
} ;
if(defined $CLIENT) {
    $title .= "<br>client: <tt>$CLIENT</tt>" ; # 
} ;
if(defined $SEARCHDESC) {
    my $not="" ;
    if(defined $SEARCH_INVERT) {
	$not = " does not"
    }
    $title .= "<br>where description$not match: <tt>$SEARCHDESC</tt>" ;
} ;
if($STATUS eq "pending") {
    $title .= "<br>(status: pending)" ;
} ;


###                                 ###
### Get changes to exclude (if any) ###
###                                 ###

local *P4 ;
my %excludeChanges ;
my $f ;
my $lastChangeInLabel = 0 ;
if(defined $EXLABEL ) {
    getChanges(-label=>$EXLABEL,
	       -resultto=> \%excludeChanges) ;
    my $n = scalar keys(%excludeChanges) ;
    my @tmp = sort { $b <=> $a } keys %excludeChanges ;
    $lastChangeInLabel = $tmp[0] ;
    &P4CGI::ERRLOG("Exclude from label \"$EXLABEL\":$n lastCh:$lastChangeInLabel") ;	

} ;

###            ###
### Start page ###
###            ###
my @legend ;
push @legend,
    "<b>Change No.</b> -- see details of change",
    "<b>User</b> -- Information about user" ;
unless(defined $SHOWREFERENCED) {    
    push @legend,&P4CGI::ahref(-url => &P4CGI::cgi()->self_url . "&SHOWREFERENCED=Y",
			       "Show description of changes referenced in change description") ;
}


&P4CGI::SET_HELP_TARGET("changeList") ;

print
    "",
    &P4CGI::start_page($title,&P4CGI::ul_list(@legend)) ;

###             ###
### Get changes ###
###             ###
my %changes ;
my $cchange = "0" ;
my $oldestSafeCh = 0 ;		# The last change in %changes that is "safe" (after this change changes
				# may be missing)
my $gotAll="No" ;		# Set to "Yes" if there are no more changes to display 
my %magicLinks ;


###
### If "pending" get all pending changes 
###
if($STATUS eq "pending") {
				# Pending. All file and label specifications ignored...
    $title = "Pending changes" ;
    my $choffstr = "" ;
    my %chs ;
    getChanges(-status => "pending",
	       -long => 1,
	       -resultto => \%changes) ;
    my $ch ;
    foreach $ch (sort { $b <=> $a } keys %changes) { 
	&setDisplay($ch, \%changes);
    }			  
} 

###
### If not "pending" get all changes
###
else {
    my $max = $MAXCH ; # max

    ##
    ## Create subroutines for selection
    ##
    my @selectFuncs ; # Variable to hold subroutines

    if(defined $SEARCHDESC) {	# Search description
	$max = (1+$max)*10 ;
	my $s = "($SEARCHDESC)" ;
#	$s =~ s/\s*\+\s*/|/g ;
	$s =~ s/\./\\\./g ;
	$s =~ s/\*/.\*/g ;
	$s =~ s/\?/./g ;
	$s =~ s/\s+/[\\\s+\n]+/g ;
	my $sq = $s ;
	$sq =~ s/\n/\\n/g ;
	&P4CGI::ERRLOG("select: ".$sq) ;
	if(defined $SEARCH_INVERT) {
	    push @selectFuncs , sub { my ($ch,$date,$user,$client,$desc) = @_ ;
				      return $$desc !~ /$s/gi ;
				  } ;
	}
	else {
	    push @selectFuncs , sub { my ($ch,$date,$user,$client,$desc) = @_ ;
				      return $$desc =~ s/$s/$MAGIC_RED$1$MAGIC_RED/gi ;
				  } ;
	}
    } ;
    if(defined $GROUP) {	# Group(s) specified
	my @grps = split(',',$GROUP) ;
	while(@grps) {
	    my $grp = shift @grps ;
	    &P4CGI::ERRLOG("group: $grp") ;	
	    my %data ;
	    &P4CGI::p4readform("group -o $grp",\%data) ;
	    if(exists $data{"Subgroups"}) {
		push @grps,split("\n",$data{"Subgroups"}) ;
	    }
	    my $u ;
	    foreach $u (split("\n",$data{"Users"}))
	    {
		if(defined $USER) {
		    $USER .= ",$u";
		}
		else {
		    $USER = "$u" ;
		}
	    }
	}	
    }
    if(defined $USER) {		# User(s) specified
	my %users ;
	my $usersToCheck = 0 ;
	foreach (split(',',$USER)) {
	    $users{$_} = 1 ;
	    $usersToCheck++ ;
	}
	&P4CGI::ERRLOG("users: ".join(",",(keys %users))) ;	
	push @selectFuncs , sub { my ($ch,$date,$user,$client,$desc) = @_ ;	
				  return exists $users{$user} ;
			      } ; 
	my @users ;
	@users=&P4CGI::run_cmd("users") ;
	$max *= 3+int(@users/(5*$usersToCheck)) ;
    } ;     
    if(defined $CLIENT) {	# Client specified
	my %clients ;
	my $clientsToCheck = 0 ;
	foreach (split(',',$CLIENT)) {
	    $clients{$_} = 1 ;
	    $clientsToCheck++ ;
	}
	&P4CGI::ERRLOG("client: $CLIENT") ;	
	push @selectFuncs , sub { my ($ch,$date,$user,$client,$desc) = @_ ;	
				  return exists $clients{$client} ;
			      } ; 
	my @clients ;
	@clients=&P4CGI::run_cmd("clients") ;
	$max *= 3+int(@clients/(5*$clientsToCheck)) ;
    } ;     
    if((keys %excludeChanges) > 0) { # Exclude changes from a list
	push @selectFuncs , sub { my ($ch,$date,$user,$client,$desc) = @_ ;	
				  return ! exists $excludeChanges{$ch} ;
			      } ; 	
    }
    ##
    ## Create a select subroutine for selection functions defined (if any)
    ##
    my $selectFunc ;
    if(@selectFuncs > 0) {
	$selectFunc = sub {
	    my @params = @_ ;
	    foreach (@selectFuncs) {
		return undef unless &$_(@params) ;
	    }
	    return 1 ;
	}
    }    

    ##
    ## Set max changes to return for each search
    ##
    $max = 2000 if $max < 2000 ; # There is no point searching less than 2000 at the time.
 			         # Absolutely no point.

    my %chLevel ; # Store how far back we have traced by file spec
    my %ended   ; # Set to true for a file spec where we have hit the end
				# HINT: We can merge changes from more than one file spec
    my $noFSPCsNotEnded = 0 ; # Store number of file speces that has not ended (so we know
			      # when there is no point to keep trying)
    
    ##
    ## Initialize variable above
    ##
    my $firstch = &P4CGI::CURRENT_CHANGE_LEVEL() ; # First change we are interested in
    $firstch = $FIRSTCH if defined $FIRSTCH ; # Set if parameter FIRSTCH given
       
    my $fspc ;    
    foreach $fspc (@FSPC) {
	$chLevel{$fspc} = $firstch ; # Set level to current for all filespec's
	$ended{$fspc}   = 0 ;	     # Not ready with flespec yet...
	$noFSPCsNotEnded++ ;	     # Increment filespecs not ready
    }

    my %chs ; # result hash
    
    while(1) {			# Loop until ready

	##
	## Loop over each file spec
	##
	$oldestSafeCh = 0 ; 
	my $filespec ;		
	foreach $filespec (@FSPC) { # for each filespec.....
	    next if $ended{$filespec} ; # Skip if all changes read for file spec
	    my $lastIncludedCh ; # Store last change included in selection
	    #
	    # Set up parameters for getChanges()
	    #
	    my %params = (-file     => $filespec ,
			  -resultto => \%changes,
			  -long     => 1,
			  -maxch    => $max+1,
			  -select   => $selectFunc,
			  -lastch   => \$chLevel{$filespec},
			  -lastrch  => \$lastIncludedCh,
			  -firstch  => $chLevel{$filespec},
			  -magiclinked => \%magicLinks ) ;
	    
	    if(defined $LABEL) {
		$params{"-label"} = $LABEL ;
	    }
	    #
	    # Call getChanges
	    #
	    my $gotCh = getChanges(%params) ;
	    #
	    # Evaluate returned data
	    #
	    if($gotCh != ($max+1)) {     # Did we get all changes we asked for? If no....
		$ended{$filespec} = 1 ;	 # ... there is no more data for this file spec
		$noFSPCsNotEnded-- ;
		&P4CGI::ERRLOG("file spec \"$filespec\" ended") ;	
	    }
	    else {
		if($lastIncludedCh > $oldestSafeCh) { # Update oldes safe ch. 
		    $oldestSafeCh = $lastIncludedCh ;
		}
	    }
	} # End loop over each filespec
	
	if($noFSPCsNotEnded == 0) { # Did we get all changes there are for the filespecs?
	    # No more changes for these filespecs
	    $gotAll = "Yes" ;
	    last ;
	}
				# Count number of changes that we can "trust" (for more than
				# one file spec we reach different number of changes back in time)
	my $okchs = 0 ;
	my $c ;
	foreach $c (keys %changes) {
	    $okchs++ if $c >= $oldestSafeCh ;
	}
	&P4CGI::ERRLOG("okchs: $okchs, max: $max") ;
	last if $okchs >= $MAXCH ; # Did we get enough changes...
    }
    
    ##
    ## Build data for changes to display
    ##
    my $changesDisplayed=0 ;
    my $ch ;
    my @sorted = sort { $b <=> $a } keys %changes ;
    while($ch = shift(@sorted)) {	
	if((exists $changes{$ch}{'display'}) or (defined $FIRSTCH and ($ch > $FIRSTCH))) {
	    &P4CGI::ERRLOG("skip ch $ch") ;
	}
	else {	    
	    &setDisplay($ch, \%changes);

	    $changesDisplayed++ ;
	    if(defined $SHOWREFERENCED and exists $magicLinks{$ch}) {
		my $refch = "<small><dl compact>" ;
		my $n = 0 ;
		my $c ;
		foreach $c (@{$magicLinks{$ch}}) {
		    my %data ;
		    &P4CGI::p4readform("change -o $c",\%data) ;
		    if(exists $data{"Description"}) {
			$n++ ;
			my $d = &P4CGI::fixSpecChar($data{"Description"}) ;
			$d =~ s/\n/<br>\n/g ;		
			$c = &P4CGI::ahref("-url" => "changeView.cgi",
					   %extraUrlOptions,
					   "CH=$c",
					   "Change $c") ;
			if(exists $data{"User"}) {
			    $c .= " by " . &P4CGI::ahref("-url" => "userView.cgi",
							 "USER=$data{User}",
							 $data{"User"}) ;
			}
			$refch .=  "<dt>$c:\n<dd><tt>$d</tt>" ;			
		    }
		}
		if($n > 0) {
		    $changes{$ch}{'display'} .= "$refch</dl></small><hr width=50% align=left>" ;
		}
	    }
	}
	if($changesDisplayed == $MAXCH) {
	    if(@sorted > 0) {
		$gotAll = "No" ;
	    }
	    last ;
	}
    }			  
} ;

sub min {
    my ($a,$b) = @_;
    if ($a < $b) {
	return $a;
    } else {
	return $b;
    }

}


sub setDisplay {
    my ($change, $chs) = @_;

    my @files = ();
    my $time = '';

    if ($SHOWFILES) {
	&getDescription($change, $chs);
 	@files= @{ $$chs{$change}{'files'} } ;
	$time = $$chs{$change}{'time'} ;
    }

    my $display;
    $display = "<dt> ". &P4CGI::ahref("-url" => "changeView.cgi",
				      %extraUrlOptions,
				      "CH=$change",
				      "Change $change") . "\n" ;
    $display .= " $$chs{$change}{'date'} $time by ";
    $display .= &P4CGI::ahref("-url" => "userView.cgi",
			      "USER=$$chs{$change}{'user'}",
			      $$chs{$change}{'user'}) . "\@" ;
    $display .= &P4CGI::ahref("-url" => "clientView.cgi",
			      "CLIENT=$$chs{$change}{'client'}",
			      $$chs{$change}{'client'}) . "\n<dd>" ;
    $display .= $$chs{$change}{'desc'} ;
    
    $display .= "@files <br>\n";

    $$chs{$change}{'display'} = $display;
    return $display;
}


###             ###
### Start print ###
###             ###

my @params = &get_params();
my @changes = (sort { $b <=> $a } keys %changes);
my $firstch = $changes[0];
my $lastch=$changes[&min($#changes,$MAXCH)];
#print "first: $firstch last: $lastch maxch: $MAXCH choffset $CHOFFSET <br>";


print  &P4CGI::ahref(-url => "bonsai.cgi" , @params,
		     "Modify Query"
		     )."<br><br>\n" ;


     
my $current_change_level = &P4CGI::CURRENT_CHANGE_LEVEL();
my $firstch = $FIRSTCH+$MAXCH+1;
if($firstch <= $current_change_level) {
    my @params = &get_params();
    print
	"",
	&P4CGI::ahref("-url","changeList.cgi",
		      @params,"FIRSTCH=".($firstch+1),
		      "CHOFFSETDISP=".($CHOFFSET-$MAXCH),
		      "<b>Previous....</b>") ;
}

print "<dl>\n" ;
my $debug_size = scalar keys %changes ;
&P4CGI::ERRLOG("$debug_size changes to display") ;
my $ch ;
my $maxch = $MAXCH ;
#my $skipped = 0 ;
foreach $ch (@changes) {    
    last if ($maxch == 0) ;
    if($ch < $oldestSafeCh) { # Can not happend???
	&P4CGI::ERRLOG("ch:$ch oldestSafeCh:$oldestSafeCh") ; # DEBUG
	$maxch = 0 ;
	last ;
    } ;
    $maxch-- ;
    $CHOFFSET++ ;
    if(defined $EXLABEL and $ch < $lastChangeInLabel) {
	print
	    "</dl><table width=90% align=center cols=3 bgcolor=",
	    &P4CGI::HDRFTR_BGCOLOR(),
	    "><tr><th width=4 align=center><hr>Last change in Label $EXLABEL is $lastChangeInLabel<hr>",
	    "</th></tr></table><dl>\n" ;
	$lastChangeInLabel = 0 ;
  }
    my $user = $changes{$ch}{'user'} ;
    $allUsers{$user} = 1;
    
    print $changes{$ch}{'display'} ;
}	

print "</dl>\n" ;

&P4CGI::ERRLOG("gotAll:$gotAll maxch:$maxch") ;
if(($maxch == 0) and ($gotAll ne "Yes")) {
    print
	"",
	&P4CGI::ahref("-url","changeList.cgi",
		      @params,"FIRSTCH=".($lastch-1),
		      "CHOFFSETDISP=$CHOFFSET",
		      "<b>More....</b>")."<br><br>\n" ;
}

print  &P4CGI::ahref(-url => "mailto:".join( ',', (sort keys %allUsers)) ,
                           ( "Mail everyone on this page". 
                             " (" . scalar(keys %allUsers) . " people)"
			     )
                             
                             )."<br>\n" ;


print  &P4CGI::ahref(-url => "bonsai.cgi" , @params,
		     "Modify Query"
		     )."<br>\n" ;


print "",&P4CGI::end_page();

#
# That's all folks
#


