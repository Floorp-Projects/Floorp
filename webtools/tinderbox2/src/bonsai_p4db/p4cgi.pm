#!/usr/bin/perl -Tw
# -*-perl-*-
##################################################################
# "P4CGI" perl module.
#
#
#
package P4CGI ;
###
# You might need to uncomment and set the lib to point to the  perl libraries
#
#use lib '/usr/local/lib/perl5/site_perl' ;

sub ConfigFileName()
{
    return "./P4DB.conf" ; # Change here to rename configuration file
}

use CGI ;
use CGI::Carp ;
use strict;
use Date::Manip;


#### Conficuration file name
my $CONFIG_FILE ;


my @ERRLOG ;
my $errors ;

###
### Subroutine that prints the error log
###
sub prerrlog() {
  my $res = "" ;
  if(@ERRLOG > 0) {
	map { 
	    if(/^ERROR:/) { $_ = "<font color=red>" . &fixSpecChar($_) . "</font>" ; } 
	    else { $_ = "<font color=blue>" . &fixSpecChar($_) . "</font>" ; } 
	  }  @ERRLOG ;
	$res .= 
	    "<p><font color=blue>Log printout:<br><pre>" .
		join("\n",@ERRLOG) .
		    "</pre></font>\n" ;
    } ;    
    return $res ;
}

#### The following constants are set or updated by the init() routine


my $VERSION ;			# P4DB version
my $P4 ;			# Contains p4 command
my $PORT;			# Contains p4 port
my $CGI;			# Contains CGI object from CGI.pm 
my $lastChange ;  	        # Current change level
my $IGNORE_CASE;		# "YES" if case should be ignored
my $MAX_CHANGES;		# Max changes for change list
my $TAB_SIZE;			# Tab size for file viewer
my $USE_JAVA;			# Defined if JAVA should be used
my $SHORTCUT_FILE;		# Name of file containing shortcuts
my @RESTRICTED_FILES ;		# Files where view is restricted
my $HTML_BGCOLOR;		# Background color
my $HTML_BACKGROUND ;		# Background picture (overrides BGCOLOR if defined)
my $HTML_TEXT_COLOR;		# Text color
my $HTML_LINK_COLOR;		# Link color
my $HTML_ALINK_COLOR;		# Active link color
my $HTML_LINK_TEXT_DEC;		# Link/vlink text decoration. 
my $HTML_TITLE1_BGCOLOR ;
my $HTML_TITLE1_COLOR   ; 
my $HTML_HDRFTR_BGCOLOR ;
my $HTML_HDRFTR_COLOR   ;
my $ICON_PATH;			# Path to icons (usually ../icons or something)
my $HELPFILE_PATH;		# Path to help file (html)
my $REDIRECT_ERROR_TO_NULL_DEVICE; # Part of command thar redirects errors to /dev/null
my $REDIRECT_ERROR_TO_STDOUT;   # Part of command thar redirects errors to stdout
my $DEBUG ;			# When true, prints log

my %PREF ;			# Preferences
my %PREF_LIST ;			# List of preferences


#### Other package variables

my $pageStartPrinted ;		# Used to avoid mutliple headers if an error occurres

my @P4DBadmin ;	# Admins for P4DB and Perforce server

my %CONF ;		# Hash containing configuration

my %EXTRAHEADER ;

my $helpTarget ;		# target for help text

sub init( )
{
    #
    # Set config file name
    #
    $CONFIG_FILE = &ConfigFileName ;
			## death handler
    $SIG{'__DIE__'} = sub {	# Thank you Ron Shalhoup for the idea
	my($error) = shift; 
	&P4CGI::bail("Signal caught: $error") ;	
	exit 0; 
    }; 

    $ENV{'PATH'} = "/bin";

    # taint perl requires we clean up these bad environmental variables.

    delete @ENV{'IFS', 'CDPATH', 'ENV', 'BASH_ENV', 'LD_PRELOAD'};

    # sudo deletes these variables as well

    delete @ENV{'KRB_CONF', 'KRB5_CONFIG'};
    delete @ENV{'LOCALDOMAIN', 'RES_OPTIONS', 'HOSTALIASES'};

    umask 0022;
    $| = 1;

    #
    # clear error counter
    #
    $errors = 0 ;

    #
    # Set version
    #
    $VERSION="2.01" ;
#    $ VERSION="2.01 (535)" ;

    #
    # Set configuration defaults
    #
    $HTML_BGCOLOR      = "#f0f0e0" ;
    $HTML_BACKGROUND   = undef ;
    $HTML_TEXT_COLOR   = "#000000" ;
    $HTML_LINK_COLOR   = "#000099" ;
    $HTML_ALINK_COLOR  = "#000099" ;
    $HTML_LINK_TEXT_DEC= "none" ;
    $SHORTCUT_FILE     = "" ;
    $ICON_PATH         = "./" ;
    $HELPFILE_PATH     = "./" ;
    $HTML_TITLE1_BGCOLOR = "#ccffff" ;
    $HTML_TITLE1_COLOR   = "blue" ;
    $HTML_HDRFTR_BGCOLOR = "#FFFF99" ;
    $HTML_HDRFTR_COLOR   = "#e02020" ;
    
    #
    # Initiate CGI module 
    #
    $CGI = new CGI ;
    $CGI->autoEscape(undef) ;

    #
    # Setup preference list
    #
    %PREF_LIST = 
	(
	 "JV"   => ["x:BOOL",
		    "Enable experimental java depot tree browser.<br>(not always available)",0],
	 "SF"   => ["b:LIST","Shortcut file to use",0],
	 "DP"   => ["a:LIST","P4 Depot",0],
	 "TB"   => ["d:INT","Default tab stop",8],
	 "MX"   => ["d:INT","Max changes to show",100],
	 "IC"   => ["d:BOOL","Ignore case (like NT)",0],
	 "NW"   => ["d:BOOL","Open changes in new window",0],
	 "ST"   => ["c:BOOL","Shortcuts on top of main page",0],
	 "UL"   => ["f:BOOL","Underline links",0],
	 "CL"   => ["f:LIST","Color schemes",0],
	 "VC"   => ["f:BOOL","View files with colors",1],
	 "DBG"  => ["z:BOOL","Print log information (for development)",0],
	 ) ;
    
				### Set preferences
    %PREF=$CGI->cookie(-name=>"P4DB_PREFERENCES") ; # First try cookie...
    my $p ;
    foreach $p (keys %PREF_LIST) {
	if(! defined $PREF{$p}) { 
	    $PREF{$p} = $ {$PREF_LIST{$p}}[2];
	    ERRLOG("Set $p!") ;
	}
    }  ;
    foreach $p (keys %PREF) {
	if(exists $PREF_LIST{$p}) {
	    ERRLOG("PREF: $p => $PREF{$p} (${$PREF_LIST{$p}}[1])") ;
	}
	else {
	    delete $PREF{$p} ;
	} ;
    } ;
    if(defined $CGI->param("SET_PREFERENCES")) {
	my $c ;
	foreach $c (keys %PREF) {
	    my $val =  $CGI->param($c) ;
	    if(defined $val) {
		my $type = $ {$PREF_LIST{$c}}[0] ;
		if($type eq "INT") {  $val =~ /^\d+$/ or next ;	} ;
		if($type eq "BOOL") { $val =~ /^[01]$/ or next ; } ;
		$PREF{$c} = $val ;
	    }
	}
    }

				### Set up data structure for configuration file read
    my %configReaderData = ( "P4PATH"         => \$P4,
			     "HTML_ICON_PATH" => \$ICON_PATH,
			     "HTML_HELPFILE_PATH" => \$HELPFILE_PATH,
			     "P4DB_ADMIN"     => \@P4DBadmin,
			     "SHELL"          => \$ENV{"SHELL"},
			     "REDIRECT_ERROR_TO_NULL_DEVICE" => \$REDIRECT_ERROR_TO_NULL_DEVICE,
			     "REDIRECT_ERROR_TO_STDOUT"      => \$REDIRECT_ERROR_TO_STDOUT,
			     "DEPOT"          => $PREF_LIST{"DP"},
			     "SHORTCUT_FILE"  => $PREF_LIST{"SF"},
			     "COLORS"         => $PREF_LIST{"CL"},
			     "\@BGCOLOR"        => $PREF_LIST{"BGC"},
			     "\@BGCOLORT"       => $PREF_LIST{"TC1"},
			     "\@BGCOLORT2"      => $PREF_LIST{"TC2"},
			     "\@TEXTCOLOR"      => $PREF_LIST{"TC"},
			     "\@LINKCOLOR"      => $PREF_LIST{"LC"},
			     ) ;
    
				### Read configuration file 
    local *F ;
    my $line = 0 ;
    open(F,"<$CONFIG_FILE") or &P4CGI::bail("Could not open config file \"$CONFIG_FILE\" for read") ;   
    while(<F>) {
	$line++ ;
	chomp ;			# Remove newline
	s/^\s+// ;		# remove leading spaces
	next if (/^\#/ or /^\s*$/) ; # Skip if comment or empty line
	s/\s+/ /g ;		# Normalize all spaces to a single space
	s/ $// ;		# Remove trailing spaces

				# Check syntax and get data
	/^(\S+):\s*(.*)/ or 
	    &P4CGI::bail("Parse error in config file \"$CONFIG_FILE\" line $line:\n\"$_\"") ;
				# Get values
	my ($res,$val) = ($1,$2);
				# Make sure config value exist
	if(! exists $configReaderData{$res}) {
	    &P4CGI::bail("Parse error in config file \"$CONFIG_FILE\" line $line:\n\"$_\"") ;
	} ;
				# Get config value and check type
	my $ref = $configReaderData{$res} ;
	my $type = ref($ref) ;
	$type eq "SCALAR" and do { $$ref = $val ; ERRLOG("$res=$val") ; next ; } ;
	$type eq "ARRAY" and do {
	    if($res =~ /^\@/) {
		push @$ref,split /\s+/,$val ;
	    }
	    else {
		push @$ref,$val ;
	    } ;
# Potetial security hole, any user can se p4 user and password	    
#	    ERRLOG("push $res,$val") ; 
	    next ;
	} ;
	
	&P4CGI::bail("Illegal config type $type line $line:\n\"$_\"") ;	
    }
    close F ;

				### Set port and p4 command
    $PORT = $ {$PREF_LIST{"DP"}}[$PREF{"DP"}+3] ;
    if(!defined $PORT) {
	$PORT = $ {$PREF_LIST{"DP"}}[3] ;
	$PREF{"DP"} = 0 ;
    }
    bail("DEPOT NOT DEFINED") unless defined $PORT ;
    
    $PORT =~ /(\S+)\s(\S+)\s(\S+)\s(\S+)/ or do { bail("DEPOT line not correct ($PORT)") ; } ;
    $PORT = $1 ;
    $P4 .= " -p $1 -u $2 -c $3 " ;
    if($4 ne "*") {
	$P4 .= "-P $4 " ;
    } ;

# Potential security hole, any user can se the log..
#    push @ERRLOG,"P4 command set to: \"$P4\"" ;
    
				### Set up shortcut file
    $SHORTCUT_FILE = $ {$PREF_LIST{"SF"}}[$PREF{"SF"}+3] ;
    if(!defined $SHORTCUT_FILE) {
	$SHORTCUT_FILE = $ {$PREF_LIST{"SF"}}[3] ;
	$PREF{"SF"} = 0 ;
    }
    $SHORTCUT_FILE =~ s/^(\S+).*/$1/ ;
    
				### Get colors
    my $colors = $ {$PREF_LIST{"CL"}}[$PREF{"CL"}+3] ;
    if(!defined $colors) {
	$colors = $ {$PREF_LIST{"CL"}}[3] ;
	$PREF{"CL"} = 0 ;
    }
    if($colors =~ /^(\S+)\s(\S+)\s(\S+)\s(\S+)\s(\S+)\s(\S+)\s(\S+)\s(\S+)\s(\S+)\s/) {
	$HTML_BGCOLOR    = $1 ;
	$HTML_TEXT_COLOR = $2 ;
	$HTML_LINK_COLOR = $3 ;
	$HTML_ALINK_COLOR = $4 ;	
  $HTML_TITLE1_BGCOLOR = $5 ;
	$HTML_TITLE1_COLOR = $6 ;
	$HTML_HDRFTR_BGCOLOR = $7 ;
	$HTML_HDRFTR_COLOR = $8 ;
    } ;
    
				### fix undelines
    if($PREF{"UL"}) {
	$HTML_LINK_TEXT_DEC = "underline" ;
    } ;
    

    $IGNORE_CASE   = $PREF{"IC"} ? "Yes" : "No" ;
    $TAB_SIZE      = $PREF{"TB"} ;
    $TAB_SIZE = 16 if $TAB_SIZE > 16 ;
    $TAB_SIZE = 0 if $TAB_SIZE <= 0 ;
    $USE_JAVA      = $PREF{"JV"} ? "Yes" : undef ;
    $MAX_CHANGES   = $PREF{"MX"} ;

    foreach (keys %ENV) {
	push @ERRLOG,"Environment variable $_: \"". $ENV{$_} . "\"" ;
    } ;

    #
    # Check that we have contact with p4 server
    #
    $lastChange= undef ;
    my @d ;
    @d=&P4CGI::run_cmd("changes","-m","1") ;
    $d[0] =~ /Change (\d+)/ and do { $lastChange=$1 ;} ;

} ;


#################################################################
### Documentation start

=head1 NAME

P4CGI - Support for CGI's that interface p4. Written specifically for P4DB

=cut ;

sub CURRENT_CHANGE_LEVEL() { return $lastChange ? $lastChange : -1 ; } ;
sub RESTRICTED()           { return @RESTRICTED_FILES ;    } ;
sub USER_P4PORT()          { return $PORT ;                } ;
sub ICON_PATH()            { return $ICON_PATH ;           } ;
sub HELPFILE_PATH()        { return $HELPFILE_PATH ;       } ;
sub REDIRECT_ERROR_TO_NULL_DEVICE() { return $REDIRECT_ERROR_TO_NULL_DEVICE ; } ;
sub REDIRECT_ERROR_TO_STDOUT() { return $REDIRECT_ERROR_TO_STDOUT ; } ;
sub SHORTCUT_FILE()        { return $SHORTCUT_FILE ;       } ;
sub VIEW_WITH_COLORS()     { return $PREF{"VC"} ;          } ;
sub SHORTCUTS_ON_TOP()     { return $PREF{"ST"} ;          } ;
sub CHANGES_IN_SEPPARATE_WIN() { return $PREF{"NW"} ;      } ;
sub USE_JAVA()             { return $USE_JAVA ;            } ;
sub MAX_CHANGES()          { return &extract_digits($MAX_CHANGES) ;         } ;
sub HDRFTR_BGCOLOR()       { return $HTML_HDRFTR_BGCOLOR ; } ;
sub BGCOLOR()              { return $HTML_BGCOLOR ;        } ;
sub ERRLOG                 { push @ERRLOG,@_ ; }; 
sub ERROR                  { &ERRLOG(map { "ERROR: $_" } @_) ; $errors++ ; }; 
sub EXTRAHEADER(% )        { %EXTRAHEADER = @_ ; } ;
sub SET_HELP_TARGET($ )    { $helpTarget = shift @_ ; } ;
sub IGNORE_CASE()          { return $IGNORE_CASE ; } ;


sub PREF()                 { return %PREF ; } ;
sub PREF_LIST()            { return %PREF_LIST ; } ;


=head1 SUBROUTINES

=cut ;

###################################################################
###  cgi
###

=head2 cgi

C<&P4CGI::cgi()>

Return CGI reference 

Example:

    my $file = P4CGI::cgi()->param("file") ;
    print "File parameter value: $file\n" ;

=cut
    ;
sub cgi() {
    confess "CGI not initialized" unless defined $CGI ;
    return $CGI ;
}


###################################################################
###  p4call
###

=head2 p4call

C<&P4CGI::p4call(>B<result>C<,>B<command>C<)>

Request data from p4. Calls p4 with command B<command> and returns data in B<result>.

This function is really three different functions depeding in the type of the
B<result> parameter.

=over 4

=item result

This parameter can be of three different types:

=over 4

=item Filehandle (typeglob)

Data from command can be read from filehandle. NOTE! File must be closed by
caller.

=item Reference to array

Returns result from command into array (newlines stripped)

=item Reference to scalar

Returns result from command into scalar. (lines separated by newline)

=back

Any other type of parameter will abort operation

=item command

Command to send to p4 command line client.

=back

Example:

    my $d ;
    &P4CGI::p4call(\$d,"changes -m 1") ;
    $d =~ /Change (\d+)/ or &bail("No contact with P4 server") ;
    $lastChange=$1 ;    

=cut
    ;

sub p4call {
    my ( $par, @command ) = @_; 
    my $partype = ref $par ;
#&bail( "$P4 @command failed" );
    push @ERRLOG,"p4call(<$partype>,@command)" ;

    $ENV{'PATH'} = "/bin";
    # Unfortunately, we can't filter out " and >, since they're used to quote
    # the arguments, and redirect output. Bummer. We'll just hope that everyone
    # else filtered things OK.
    &bail("Invalid P4 command @command") if ("$P4 @command" =~ /['`&;|<\!()\$\\]/);
    if(!$partype) {
	open( $par, "$P4 @command|" ) || &bail( "$P4 @command failed" );
	return ;
    }  ;
    "ARRAY" eq $partype and do {
	local *P4 ;
	@$par = () ;
	open( P4, "$P4 @command|" ) || &bail( "$P4 @command failed" );
	while(<P4>) {
	    chomp ;
	    push @$par,$_ ;
	} ;
	close P4 ;
	return ;
    } ;
    "SCALAR" eq $partype and do {
	$$par = "" ;
	local *P4 ;
	open( P4, "$P4 @command|" ) || &bail( "$P4 @command failed" );
	while(<P4>) {
	    $$par .= $_ ;
	} ;
	close P4 ;	
	return ;
    } ;
    die("Called with illegal parameter ref: $partype") ;
} ;

###################################################################
###  p4readform
###

=head2 p4readform

C<&P4CGI::p4readform(>B<command>,B<resulthash>C<)>

Reads output from a P4 command and assumes the data is a form (e.g. "client -o").

The form is stored in a hash and the function returns an array containing all
field names in the order they appeared. The hash will contain the field names as
key and field values as data.

=over 4

=item command

Command to send to p4 command line client.

=item resulthash

Reference to a hash to receive reults

=back

Example:

    my %fields ;
    my @fields = &P4CGI::p4readforml("client -o",\%fields) ;
    my $f ;
    foreach $f (@fields) {
        print "field $f: $fields{$f}\n" ;
    }

=cut
    ;

sub p4readform($\% )
{
    my $cmd = shift @_ ;
    my $href = shift @_ ;
    my @result ;
				# clear hash
    %$href = () ;

    local *F ;
    p4call(*F,$cmd) ;
    my $cline = <F>;
    while($cline) {
	chomp $cline ;
	$_ = $cline ;
	if(/^\#/ or /^\s*$/) {	# skip comments and empty line
	    $cline = <F>;
	    next ;
	}
	if(/^(\S+):\s*(.*)\s*$/) {
	    my $fld=$1 ;
	    my $val=$2 ;
	    push @result,$fld ;
	    my $ws ;
	    if($val eq "") {
		$val = undef ;
		while(defined ($cline = <F>)) {
		    $_ = $cline ;
		    chomp ;
		    last if /^\w/ ;
		    s/^\s+// ;
		    if(defined $val) {
			$val .= "\n$_" ;
		    }
		    else {
			$val = "$_" ;
		    }
		}
	    }
	    else {
		$cline = <F>;
	    }
	    $$href{$fld}=$val ;
	}
	else {
	    $cline = <F>;
	}
    }
    close *F ;
    return @result ;
} ;

###################################################################
###  start_page
###

=head2 start_page

C<&P4CGI::start_page(>B<title>[C<,>B<legend>]C<)>

Start a page. Print http header and first part of HTML.

=over 4

=item title

Title of page

=item legend (Optional)

Short help text to be displayed at top of page

=back

Example:

 my $start = P4CGI::start_page("Title of page",
			       &P4CGI::dl_list("This","Goto this",
                                               "That","Goto that")) ;
 print $start ;

=cut ;

sub start_page($;$)
{
    my $title  = shift @_ ;
    my $legend = shift @_ ;
    $legend = "" unless defined $legend ;
    my $n = 0 ;
    
    my $helpURL="${HELPFILE_PATH}/P4DB_Help.cgi" ;
    if(defined $helpTarget) {
	$helpURL .= "#$helpTarget" ;
    } ;
    
    my $p4port = "" ;
    $p4port = join("\n",("<small><table>",
			 "  <td align=center>",
			 "<small>Click here for<br></small>",
			 ahref(-url=>$helpURL,
			       "<font size=+1><B>Help</B></font>"),
			 "  </td></tr>",
			 "</table></small>\n")) ;

				# Set up cookie and print header
    my $prefcookie = $CGI->cookie(-name=>"P4DB_PREFERENCES",
				  -value=>\%PREF,
				  -expires=>'+6M');
    my $ret = $CGI->header(-cookie=>$prefcookie,
			   %EXTRAHEADER). "\n"  ;
    
    my $t = "$title" ;		# Take title and removed all HTML tags 
    $t =~ s/<br>/ /ig ;
    $t =~ s/<[^>]*>//g ;
    
    my %header ;		# Fill in header fields
    $header{"-title"}  = "P4DB: $t" ;
    $header{"-author"} = "fredric\@mydata.se" ;
    $header{"-bgcolor"} =  $HTML_BGCOLOR ;
    $header{"-background"} = $HTML_BACKGROUND if defined $HTML_BACKGROUND ;
    $header{"-text"} =  $HTML_TEXT_COLOR ;
    $header{"-style"} = join("\n",("A ",
				   "{color:$HTML_LINK_COLOR; text-decoration:$HTML_LINK_TEXT_DEC;}",
				   "A:hover",
				   "{color:$HTML_ALINK_COLOR; text-decoration:underline;}",
				   "BODY, TABLE, TD, TH, TR, P ",
				   "{font-family:      Arial, Helvetica ; }")) ;
    $ret .=  $CGI->start_html(%header) ;
   
    if(defined $lastChange) {
	$ret .= start_table("width=100% bgcolor=\"$HTML_HDRFTR_BGCOLOR\" border=0 cellspacing=0 cellpadding=4") ;
	$ret .= table_row(-valign  => "top",
			  {-align  => "center",
			   -valign => "center",
			   -width  => "20%",
			   -text   => join("\n",("<a name=pagetop></a>",
						 "<font color=$HTML_HDRFTR_COLOR>",
						 "  <B>P4DB </B><i><small>Ver. $VERSION</small></i><br>",
						 "  <small>Current change level:</small> $lastChange",
						 "</font>"))},
			  {-align  => "center",
			   -valign => "center",
			   -width  => "60%",
			   -bgcolor=> "$HTML_TITLE1_BGCOLOR",
			   -text   => "<font size=+1 face=\"Arial,Helvetica\" color=$HTML_TITLE1_COLOR><b>$title</b></font>\n"},
			  {-align  => "center",
			   -valign => "center",
			   -width  => "20%",
			   -text   => "<font color=$HTML_HDRFTR_COLOR>$p4port</font>"}) ;
	my $leg = "" ;
	$leg = "<font face=\"Arial,Helvetica\">" . $legend . "</font>" if defined $legend ;
	my $homelink = "&nbsp;";
	if($CGI->url(-relative=>1) ne "index.cgi") {
	    $homelink = ahref(-url=>"index.cgi","Back To<br>Main Page") ;
	} ;
	$ret .= table_row(-bgcolor => "$HTML_HDRFTR_BGCOLOR",
			  undef,
			  {-align  => "left",
			   -text   => $leg},
			  {-align  => "right",
			   -valign => "bottom",
			   -text   => $homelink}) ;
	$ret .= end_table() ;
	$pageStartPrinted = 1 ;
	return $ret . "\n" ;    
    }
    else {
	return $ret . "<HR><H1 align=center><FONT COLOR=red><blink>No contact with p4 server<br><Font face=Courier>$PORT</font></blink></FONT></H1><HR>\n" ;
    }
} ;
    
###################################################################
###  end_page
###

=head2 end_page

C<&P4CGI::end_page()>

End a page. Print HTML trailer.

Example:

 print P4CGI::end_page() ;

=cut ;

sub end_page()
{
    my $Padmin="" ;
    my $PadminPrompt="" ;
    if(@P4DBadmin > 0) {
	$PadminPrompt = "P4DB administrator" ;
	if(@P4DBadmin > 1) {
	    $PadminPrompt .= "s" ;
	}    
	my $a ;	
	foreach $a (@P4DBadmin) {
	    push @ERRLOG,"P4DB admin: $a" ;
	    my ($email,@name) = split /\s+/,$a ;
	    my $name = join(' ',@name) ;
	    $Padmin .= "<a href=\"mailto:$email\">$name</a><br>" ;
	}
    } ;
    my $e = "" ;
    if($PREF{"DBG"} or $errors > 0) {
	if($errors > 0) {
	    $e = "<P><font color=red size=+2>$errors has occurred. Printing log information</font>" ;
	}
	$e .= prerrlog() ; 
    } ;

    my ($host,$port) = split /:/,$PORT ;
    return join("\n",
		("<table width=100% bgcolor=white cellspacing=0 cellpadding=0>",
		 "  <tr><td>",
		 "    <table bgcolor=\"$HTML_HDRFTR_BGCOLOR\" width=100% border=0 cellspacing=0 cellpadding=0>",
		 "      <tr><td align=left>",
		 "<table>",
		 "  <tr><th align=right >",
		 "    <font color=$HTML_HDRFTR_COLOR>",
		 "      Host:<br>",
		 "      Port:</font></th>",
		 "  <td>",
		 "    <font color=$HTML_HDRFTR_COLOR>",
		 "      $host<br>",
		 "      $port",
		 "    </font>",
		 "</td></tr></table>",
		 " </td><td align=right>",
		 "        <table width=70%  border=0 cellspacing=2 cellpadding=4>",
		 "          <tr><td align=right valign=top >",
		 "            <small><font face=\"Arial,Helvetica\">$PadminPrompt:</font></small>",
		 "          </td>",
		 "          <td valign=top>",
		 "            <small><font face=\"Arial,Helvetica\">", $Padmin, "</font></small>",
		 "          </td>",
		 "          <td valign=center align=right>",
		 "            <a href=#pagetop>Top Of<br>Page</a>",
		 "          </td></tr>",		 
		 "        </table>",
		 "      </td></tr>",
		 "    </table>",
		 "  </td></tr>",
		 "</table>",
		 $e,
		 $CGI->end_html())) ;
} ;

###################################################################
### bail
###

=head2 bail

C<&P4CGI::bail(>B<message>C<)>

Report an error. This routine will emit HTML code for an error message, print the error log  and exit.

This rouine is intended to report internal errors in the code (much like assert(3) in c). 

=over 4

=item message
Message that will be displayed to user

=back

Example:

 unless(defined $must_be_defined) { 
     &P4CGI::bail("was not defined") ; 
 } ;

=cut ;

my $bailed ;

sub bail {
    unless(defined $bailed) {
	$bailed = 1 ;
	my $message = shift @_ ;
	my $text = shift @_ ;    
	unless(defined $pageStartPrinted) {
	    print 
		"",
		$CGI->header(),
		$CGI->start_html(-title   => "Error in script",
				 -bgcolor => "white");
	    $pageStartPrinted = 1 ;
	} ;    
	$message = &fixSpecChar($message) ;
	print 
	    "<br><hr color=red><p align=center><font color=red size=+2>An error has occurred<br>Sorry!</font><p><font color=red>Message:<BR><pre>$message</pre><br>" ;
	if(defined $text) { 
	    $text = &fixSpecChar($text) ;
	    print "<pre>$text</pre><br>\n" ; 
	} ;
	print
	    "<p>Parameters to script:<br>",
#	    $CGI->dump() ;
	print "</font>\n",prerrlog(), end_page() ;	
	die($message) ;
    }
} ;

###################################################################
### signalError
###

=head2 signalError

C<&P4CGI::signalError(>B<message>C<)>

Report an operator error in a reasonable fashion.
SignalError can be called before or after start_page() but if it is called 
before start_page() a "default" page header will appear. It is recommended 
to call signalError() after start_page() to make it more obvious to the
operator what the problem was.

=over 4

=item message
Message that will be displayed to user

=back

Example:

 unless(defined $must_be_defined) { 
     &P4CGI::signalError("was not defined") ; 
 } ;

=cut ;

sub signalError {
    my $message = shift @_ ;
    my $text = shift @_ ;    
    unless(defined $pageStartPrinted) {
	print "",start_page("Error","") ;
	$pageStartPrinted = 1 ;
    } ;    
    $message = &fixSpecChar($message) ;
    print 
	"<p align=center><font color=red size=+2>$message</font><br><br>" ;
    if(defined $text) { 
	$text = &fixSpecChar($text) ;
	print "<pre>$text</pre><br>\n" ; 
    } ;
    print "", end_page() ;	
    exit 0 ;
} ;

###################################################################
### help_link
###  
sub help_link($ ) {
    my $helpURL="$HELPFILE_PATH/P4DB_Help.cgi#" . shift @_ ; ;
    return ahref(-url=>$helpURL,
		 "<font size=+2 style=fixed><B>?</B></font>") ;
}


###################################################################
### start_table
###  

=head2 start_table

C<&P4CGI::start_table(>B<table_attribute_text>C<)>

Start a table with optional table attributes

=over 4

=item table_attribute_text

This text will be inserted as attributes to table tag

=back

Example:

    print P4CGI::start_table("align=center border") ;

=cut ;
sub start_table
{
    my $attribs = shift @_ ;
    my $ret = "<table " ;
    if($attribs) {
	$ret .=  " $attribs" ;
    }
    return $ret . ">\n";
}

###################################################################
### end_table
###  

=head2 end_table

C<&P4CGI::end_table()>

Return end of table string. (trivial function included mostly for symmetry)

=cut ;

sub end_table() 
{
    return "</table>\n" ;
}

###################################################################
### table_row
###

=head2 table_row

C<&P4CGI::table_row(>B<options>C<,>B<listOfValues>C<)>

Insert a row in table.

=over 4

=item options

A list of key/value pairs (a hash will do just fine) containing options for 
the row. 

The key must start with a "-".

Most key/value pairs are treated as attributes to the <TR>-tag.
The following keys are recognized as special:

=over 4

=item C<-type>

Type of cells. Default is <TD>-type. 

=item C<->I<anykey>

I<anykey> will be assumed to be a row option and will be inserted in the TR-tag. 
The value for the option is the key value, unless value is empty or undefined, in which
case the option anykey is assumed to have no value.

=back

=item listOfValues

Row data. Remaining values are assumed to be data for each cell. 
The data is typically the text in the cell but can also be:

=over 4

=item undef

An undefined value indicates that the next cell spans more than one column. 

=item Reference to a hash

The hash contains two keys: "-text" for cell text and "-type" for cell type.
All other key/value pairs are treated as attributes to the <TD> or <TH> tag.

=back

=back

Example:

 print P4CGI::start_table("align=center") ;
                                   ### print header row
 print P4CGI::table_row(-type   => "th",
			-valign => "top",
			-align  => "left",
                        "Heading 1","Heading 2",undef,"Heading 3") ;
                                   ### print data
 my %h = (-text    => "text in hash", 
	  -bgcolor => "blue") ;
 print P4CGI::table_row(-valign  => "top",
			-bgcolor => "white",
                        "Cell 1",
			{-text    => "Cell 2",
			 -bgcolor => "red"},
			\%h,
			"Cell 3-2") ;
 print P4CGI::end_table() ;

=cut ;

sub table_row 
{   
    confess ("P4CGI::table_row() Parameters required!") if @_ == 0 ;
    my @ret ;
    my $n = 0 ;
    my $ec = 0 ;
    my $option = shift @_ ;
    my %options ;
    while(defined $option and ($option =~ s/^-//)) {	
	confess ("P4CGI::table_row() Option value required!") if @_ == 0 ;
	$options{lc($option)} = shift @_ ;
	$option = shift @_ ;
    }
    unshift @_,$option ;
    
    my $type      = "td" ;
    $type = $options{"type"} if defined $options{"type"} ;
    delete $options{"type"} ;

    push @ret,"<tr" ;
    my $attr ;
    foreach $attr (keys %options) {
	push @ret," $attr" ;
	if($options{$attr}) {
	    push @ret,"=$options{$attr}" ;
	}
    }
    push @ret,">\n" ;

    my $colspan = 0 ;
    my $cell ;
    foreach $cell (@_) {
	$colspan++ ;
	if(defined $cell) {
	    my $COLSPAN="colspan=$colspan" ;
	    $colspan=0 ;
	    if(ref $cell) {
		my $reftyp = ref $cell ;
		"HASH" eq $reftyp and do { my $txt = $$cell{"-text"} ;
					   confess "P4CGI::table_row() Missing text argument" 
					       unless defined $txt ;
					   delete $$cell{"-text"} ;
					   my $tp = $type ;
					   $tp = $$cell{"-type"} if defined 
					       $$cell{"-type"} ;
					   delete $$cell{"-type"} ;
					   push @ret,"<$tp $COLSPAN" ;
					   my $attr ;
					   foreach $attr (keys %$cell) {
					       ($a = $attr) =~ s/^-// ;
					       push @ret," $a=$$cell{$attr}" ;
					   }
					   push @ret,">$txt</$tp>\n" ;
					   next ; } ;
		confess "Illegal cell data type \"$reftyp\"" ;
	    } 
	    else {
		push @ret,"<$type $COLSPAN>$cell</$type>\n" ;
	    }
	}
    }
    push @ret,"</tr>\n" ;
    return join("",@ret) ;
}

###################################################################
### table_header
###

=head2 table_header

C<&P4CGI::table_header(>B<list of label/hint>C<)>

Create a table header row with a a description and an optional hint for each column. 

=over 4

=item list of label/hint

A list of column labels optionally followed by a '/' and a hint.

=back

Example:

 print P4CGI::start_table("align=center") ;
                                   ### print header row
 print P4CGI::table_header("File/click for story","Revision/click to view") ;
                                   ### print data
 my %h = (-text    => "text in hash", 
	  -bgcolor => "blue") ;
 print P4CGI::table_row(-valign  => "top",
			-bgcolor => "white",
                        "Cell 1",
			{-text    => "Cell 2",
			 -bgcolor => "red"},
			\%h,
			"Cell 3-2") ;
 print P4CGI::end_table() ;

=cut ;

sub table_header
{   
    my @cols ;
    my @tmp = @_ ;
    my $tmp ;
    my $n ;
    while(@tmp > 0) {	
	$tmp = shift @tmp ;
	if(defined $tmp) {
	    my $label = $tmp ;
	    my $hint = "&nbsp;" ;
	    if($label =~ s|/(.*)$||) {
		$hint = "($1)" ;
	    } ;	
	    push @cols,"<B>$label</B><BR><small>$hint</small>" ;
	}
	else {
	    push @cols,$tmp ;
	}
    }
    return table_row(@cols) ;
} ;

###################################################################
### Make a list
###

=head2 ul_list

C<&P4CGI::ul_list(>B<list>C<)>

Return a bulleted list.

=over 4

=item I<list>

Lits of data to print as bulleted list

=back

Example:

 print P4CGI::ul_list("This","is","a","bulleted","list") ;

=cut ;

sub ul_list(@ ) 
{
    my @ret ;
    if($_[0] eq "-title") {
	shift @_ ;
	push @ret, shift @_ ;
    }
    push @ret,"<ul>\n" ;
    my $a ;
    foreach $a (@_) {
	push @ret,"<li>$a\n" ;
    }
    push @ret,"</ul>\n" ;
    return join("",@ret) ;
}

###################################################################
### Make a dl list
###

=head2 dl_list

C<&P4CGI::dl_list(>B<list_of_pairs>C<)>

Returns a definition list.

=over 4

=item list_of_pairs

List of data pairs to print as a definition list. 
A hash will do just fine, only you have no control over the order in the list. 

=back

Example:

 print P4CGI::dl_list("This","Description of this",
                      "That","Description of that") ;

=cut ;

sub dl_list
{
    my @ret ;
    if($_[0] eq "-title") {
	shift @_ ;
	push @ret,shift @_ ;  
    }
    if($_[0] eq "-compact") {
	push @ret,"<dl compact>\n" ;
	shift @_ ;
    } 
    else {
	push @ret,"<dl>\n" ;
    }
    while(@_ > 1) {
	push @ret,"<dt>",shift @_,"<dd>",shift @_,"\n" ;
    }
    push @ret,"</dl>\n" ;
    return join("",@ret) ;
}

###################################################################
### Fix some special characters
###

=head2 fixSpecChar

C<&P4CGI::fixSpecChar(>B<str>C<)>

Convert all '>' to "C<&gt;>", '<' to "C<&lt;>" and '&' to "C<&amp;>".

=over 4

=item str

String to convert

=back

Example:

    my $cvstr = &P4CGI::fixSpecChar("String containing <,> and &") ;

=cut ;

sub fixSpecChar($ )
{    
    my $d = &rmTabs(shift @_) ;
    $d =~ s/&/&amp;/g ; # & -> &amp;
    $d =~ s/\"/&quot;/g;# " -> &quot;
    $d =~ s/</&lt;/g  ; # < -> &lt;
    $d =~ s/>/&gt;/g  ; # > -> &gt;
    return $d ;
}

###################################################################
### Replace tabs with spaces
###

=head2 rmTabs

C<&P4CGI::rmTabs(>B<str>C<)>

Returns B<str> with all tabs converted to spaces

=over 4

=item I<str>

String to convert

=back

=cut ;

sub rmTabs($ )
{    
    # This algorithm is kind of, well, the first thing I came up
    # with. Should be replaced with a smarter (== more efficient)
    # eventually.......
    my $l = shift @_ ;    
    if($l =~ /\t/) {
	my $tabsz=$TAB_SIZE ;
	$tabsz = 8 unless $tabsz ;
	my $pos = 0 ;
	$l = join('',map
		  { 
		      if($_ ne "\t") {
			  $pos++ ;	
			  $_  ;
		      } else
		      {
			  my $p = $pos % $tabsz ;
			  $pos += $tabsz-$p ;
			  substr("                ",0,$tabsz-$p) ;
		      } ;
		  } split('',$l)) ;
	# For those that wonder what is going on:
	# 1.  Split string to an array (of characters)
	# 2.  For each entry of array, map a function that returns value
	#     for entry or, if value is <TAB>, returns a number of spaces
	#     depending on position in string
	# 3.  Make string (scalar) of array returned from map using join().
    }
    return $l ;
}

###################################################################
### Create a href tag
###

=head2 ahref

C<&P4CGI::ahref(>B<options>C<,>B<parameters>C<,>B<text>C<)>

Returns a <A HREF...>...</A> tag pair.

=over 4

=item I<options>

Optional list of option-value pairs. Valid options are:

=over 4

=item C<-url>

Url for link. Default is current.

=item C<-anchor>

Anchor in url. Default is none.

=back

Any non-valid option marks the end of the options

=item I<parameters>

Optional list of parameters for link.

=item I<text>

The last parameter is used as text for link. 

=back

Example:

    print &P4CGI::ahref("Back to myself") ; # link to this. No parameters.

    print &P4CGI::ahref("-url","www.perforce.com",
			"To perforce") ; # link to perforce

    print &P4CGI::ahref("-anchor","THERE",
			"Go there") ; # link to anchor THERE

    print &P4CGI::ahref("-url","changeList.cgi",
			"FSPC=//.../doc/...",
			"Changes for all documentation") ; # url with parameter

=cut ;

sub ahref
{    
    my $args=@_ ;
    
    my @tmp = @_ ;
    my $url = $ENV{SCRIPT_NAME} ;
    my $anchor = "" ;
    my $pars   = "" ;
    my $params = "" ;
    while($tmp[0] =~ /^-/) {
	$tmp[0] =~ /^-url$/i and do {
	    shift @tmp ;
	    $url = shift(@tmp) ;
	    next ;
	} ;
	$tmp[0] =~ /^-anchor$/i and do {
	    shift @tmp ;
	    $anchor = "#" . shift @tmp ;
	    next ;
	} ;
	$tmp[0] =~ /^-(.*)/ and do {
	    my $p = $1 ;
	    shift @tmp ;
	    my $v = shift @tmp ;
	    $params .= " $p=$v" ;
	    next ;
	} ;
	last ;
    }
    
    while(@tmp > 1) {
	if(length($pars) > 0) {
	    $pars .= "&" ;
	}
	else {
	    $pars = "?" ;
	} ;
	$pars .= fixspaces(shift @tmp) ;
    } ;
    my $txt = shift @tmp ;
    $pars =~ s/ /\+/g ;
    return "<a href=\"${url}${pars}${anchor}\"$params>$txt</a>" ;
}

###################################################################
### Insert image
###

=head2 image

C<&P4CGI::image(>B<image>[C<,>B<text>]C<)>

Returns <IMG>-tag

Example:

    &P4CGI::image("picture.gif","Picture Here") ;

=cut ;

sub image
{    
    my $img = shift @_ || bail("P4CGI::image called without parameters!") ;
    my $text = shift @_ ;
    if($text) {
	$text = " alt=\"$text\"" ;
    }
    else {
	$text = "" ;
    }
    return "<IMG src=$ICON_PATH/$img$text border=0>" ;
}

###################################################################
### Set magic buttons
###

=head2 magic

C<&P4CGI::magic(>B<text>C<)>

Returns B<text> with some magic "patterns" substituted with links.

Currently the pattern "change I<number>" (and some variants) is replaced with a link to the
change browser.

Example:

    my $t = "Integrated change 4711 to this codeline" ;

    print &P4CGI::magic($t) ; # inserts a link to change 4711

=cut ;
sub magic($;\@)
{    
    my $t = shift @_ ;
    my %found ;
    my $res = "" ;
    my $hot = 0 ;
    my $max = &P4CGI::CURRENT_CHANGE_LEVEL() ;
    while($t =~ s/^([\s\n]*)(no\.|\.|ch\.|[a-zA-Z-0-9]+|[^a-zA-Z-0-9]+)//i) {
	$res .= $1 ;
	my $tok = $2 ;	
	if($hot == 0) {
	    $hot = 3 if $tok =~ /^(ch[\.]?|change|integrate|submit)/i ;
	}
	else {
	    $hot-- ;
	    if($tok =~ /^\d+$/ and !($t =~ /\.\d+/)) {
		if($tok > 0 and $tok < $max) {
		    $hot = 3 ;
		    $found{$tok} = 1 ;
		    $tok = ahref(-url => "changeView.cgi",
				 "CH=$tok",
				 "<b>$tok</b>") ;
		}		
	    }
	    elsif($tok eq ".") {
		$hot = 0 ;
	    }	    
	}
	$res .= $tok ;	
    } ;
    $res .= $t ;

    my $ar ;
    if($ar = shift @_) {
	@$ar = sort { $a <=> $b } keys %found ;
    } ;
    return $res ;
}

###################################################################
### Fixspaces
###

=head2 fixspaces

C<&P4CGI::fixspaces(>B<text>C<)>

Returns B<text> with characters like space substituted with "%<ASCII value>".

Example:

    my $t = "/File with spaces" ;

    print &P4CGI::fixspaces($t) ; # prints: /File%20with%20spaces

=cut ;
sub fixspaces($)
{    
    my $t = shift @_ ;
    $t =~ s/%(?![\da-fA-F][\da-fA-F])/%25/g ;
    $t =~ s/\?/%3f/g ;
    $t =~ s/&/%26/g ;
    $t =~ s/ /%20/g ;
    $t =~ s/;/%3b/g ;
    $t =~ s/\+/%2b/g ;
    $t =~ s/-/%2d/g ;
    $t =~ s/_/%5f/g ;
    $t =~ s/~/%7e/g ;
    return $t ;
}

###################################################################
### BEGIN
###
#sub BEGIN ()
#{
#    init() ;
#} ;



# ---------- 

# The 'extract' functions will untaint their input data and return
# only data which meets the extraction criterion.

# ----------


# remove unprintable characters from a string

# this is for untainting data which could be almost anything but
# should not be 'binary' data.

sub extract_printable_chars {
  my ($str) = @_;

  if (! defined($str)) {
    return '';
  }
  
  $str =~ s/^\s+//;
  $str =~ s/\s+$//;
    
  # a complete list of printable characters.

  $str =~ s![^a-zA-Z0-9\ \t\n\`\"\'\;\:\,\?\.\-\_\+\=\\\|\/\~\!\@\#\$\%\^\&\*\(\)\{\}\[\]\<\>]+!!g;

  my $out;
  if ( $str =~ m!(.*)!s ) {
      $out = $1;
  } else {
      $out = '';
  }

  return $out;
}


# remove characters are not digits from a string
sub extract_digits {
  my ($str) = @_;

  if (! defined($str)) {
     return '';
  }
        
  $str =~ s/^\s+//;
  $str =~ s/\s+$//;

  my $out;
  if ( $str =~ m/([0-9]+)/ ) {
      $out = $1;
  } else {
      $out = '';
  }

  return $out;
}


# remove characters which do not belong in a filename/static URL from
# a string

sub extract_filename_chars {
  my ($str) = @_;

  if (! defined($str)) {
     return '';
  }
        
  $str =~ s/^\s+//;
  $str =~ s/\s+$//;
  
  my $out;

  # This may be the output of a glob, make it taint safe.  We
  # should have full control over our own filenames, we do put all
  # printable characters in filenames.

  if ( $str =~ m/([0-9a-zA-Z\.\*\-\_\/\ ]+)/ ) {
      $out = $1;
  } else {
      $out = '';
  }

  return $out;
}

 


# remove characters which do not belong in a mail addresses, or CVS
# authors or Unix log int ids from a string


sub extract_user {
  my ($user) = @_;

  if (! defined($user)) {
    return '';
  }
        

  $user =~ s/^\s+//;
  $user =~ s/\s+$//;
  
  # If the data looks like this: 
  # 	' "leaf (Daniel Nunes)" <leaf@mozilla.org> ' 
  # extract the mail address.

  if ( $user =~ m/<([^>]+)>/ ) {
    $user = $1;
  }

  # At mozilla.org authors are email addresses with the "\@"
  # replaced by "\%" they have one user with a + in his name

  my $out;
  if ( $user =~ m/([a-zA-Z0-9\_\-\.\%\+\@]+)/ ) {
      $out = $1;
  } else {
      $out = '';
  }

  return $out;
}

# Due to abstraction issues, sometimes the same command (and
# arguments) is issued at several different parts of the program.
# Since we have just run the command we can use the previous results.
# The cache is not saved to disk so the cache is 'cleared' during each
# execution of the program.

sub run_cmd {
  my @cmd = @_;

  # good choices for this are '#', '\', '"', '|', ';'
  @cmd = (split(' ', $P4), @cmd);
  my ($join_char) = '!';


  # We need to run the command

  my ($pid) = open(CMD, "-|");
  
  # did we fork a new process?

  defined ($pid) || 
    die("Could not fork for cmd: '@cmd': $!\n");

  # If we are the child exec. 
  # Remember the exec function returns only if there is an error.

  ($pid) ||
    exec(@cmd) || 
      die("Could not exec: '@cmd': $!\n");

  # If we are the parent read all the childs output. 

  my @cmd_output = <CMD>;
  
  close(CMD) || 
    die("Could not close exec: '@cmd': \$?: $? : \$\!: $!\n");

  ($?) &&
    die("Could not cmd: '@cmd' exited with error: $?\n");

  return @cmd_output;
}


sub DateStr2Time {
    my ($dateStr) = @_;
    my $date;
    
    if ($dateStr =~ m/^\d+$/) {
       $date =  &ParseDateString($dateStr,"epoch %s");
    } else {
       $date = &ParseDate($dateStr);
    }
    
    if (! $date) {
     #  die "DateStr2Time: Invalid date format. '$dateStr'\n";
    }
    
    my $out = &UnixDate($date,"%Y/%m/%d %H:%M:%S");
    return $out;
  
}

sub DateList2Time {
    my ($weeks, $days, $hours) = @_;
    my $dateStr = "- ${weeks}weeks ${days}days ${hours}hours ";
   
    my $err;
    my $dateThen = &DateCalc("now",$dateStr,\$err);
    my $dateNow = &ParseDate("now");


    if (! $dateThen) {
         die "DateList2Time: Invalid date format. '$dateStr'\n";
    }
            
    my $PerforceDateStr = (
                           "\@".
                           &UnixDate($dateThen,"%Y/%m/%d %H:%M:%S").
                           ",\@".
                           &UnixDate($dateNow, "%Y/%m/%d %H:%M:%S")
                           );


    return $PerforceDateStr;
}

sub root {

   my @depots;
   my $err2null = &P4CGI::REDIRECT_ERROR_TO_NULL_DEVICE() ;
   &P4CGI::p4call(\@depots,"dirs \"//*\" $err2null") ;
   my $moreThanOneDepot = (@depots > 1) ;

   # Set server ROOT
   my $root ;
   if($moreThanOneDepot) {
        $root = "/" ;
   }
    else {
         $root = "$depots[0]/" ;
         $root =~ s|//|/|g ;
   } ;

                                    
   return $root;
}

        
###
### Figure out "back" buttons
###

sub back_buttons {
    my ($fspc) = @_;

    $fspc =~ s/\.\.\.$// if defined $fspc ;
    $fspc = &P4CGI::root() unless defined $fspc ;
    $fspc = "/$fspc/" ;

    while($fspc =~ s|//|/|) {} ;

    my $hidedel ="NO";

    my $root = root();
    my $weAreAtROOT = ($fspc eq $root) ;
    

    my @back;

    my $tmp = $fspc;       # Copy arg
    $tmp =~ s|([^/]+)/$|| ;   # Remove last subdir
    unshift @back,"/".$1; 

    while($tmp ne $root) {
        $tmp =~ s|([^/]+)/$|| or last ;
        my $f = $1 ;
        unshift @back,&P4CGI::ahref("-url"=>"depotTreeBrowser.cgi",
                                    "FSPC=$tmp$f",
                                    "HIDEDEL=$hidedel",
                                    "/$f") ;
     } ;
     unless($weAreAtROOT) {
         unshift @back,&P4CGI::ahref("-url"=>"depotTreeBrowser.cgi",
                                     "FSPC=$root",
                                     "HIDEDEL=$hidedel",
                                     "[ROOT]") ;
    }

   return @back;
   #, "  $d   weAreAtROOT $weAreAtROOT root $root fspc $fspc";
}


sub BEGIN ()
{
    init() ;
} ;


    

1;



