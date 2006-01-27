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
#  P4 depot browser, top
#
#################################################################

print "",
    &P4CGI::start_page("P4DB<br> P4 Depot Browser",
		       "<CENTER><SMALL>Hint:".
		       "You can bookmark any page you want to get back to later</SMALL></CENTER>") ;

				# Read and parse shortcut file

my $shortcut_file = &P4CGI::SHORTCUT_FILE() ;
my $SHORTCUTS="" ;

if(defined $shortcut_file and -r $shortcut_file) {	
				# Read file
    local *D ;
    open(*D, "<$shortcut_file") or &P4CGI::bail("Failed to open $shortcut_file for read") ;
    my $tmps = join("\n",<D>) ;
    $tmps =~ s/^#.*$//gm ;
    close *D ;
    my $shortcut_text = "" ;
				# Find all <P4DB [par=data...]>...</P4DB> 
    while($tmps =~ s/^(.*?)<p4db\s+(.*?)\s*>(.*?)<\/P4DB>//is) {
	$shortcut_text .= $1 ;
	my $pars = $2 ;
	my $text = $3 ;	
				# Extract arguments for <P4DB>
	my %pars ;
	while($pars =~ s/(\S+)\s*=\s*(?:"(.*?)"|(\S+))\s*//) {
	    my $par = $1 ;
	    my $val ;
	    if(defined $2) {
		$val = $2 ;
	    }
	    else {
		$val = $3 ;
	    } ;
	    $pars{uc($par)} = $val;	    
	} ;
	my $type = "" ;
	if ( defined $pars{"TYPE"} ) {
	    $type = uc($pars{"TYPE"});
	    delete $pars{"TYPE"};
	}
	my $url ;
	if($type eq "CHANGELIST") { 
	    $url = "changeList.cgi" ; 
	} ;
	if($type eq "BROWSE") { 
	    $url = "depotTreeBrowser.cgi" ;
	} ;
	if($type eq "JOBLIST")    { 
	    $url = "jobList.cgi" ; 
	    my %flds ;
	    &P4CGI::p4readform("jobspec -o",\%flds) ;
	    my %fldtrans = map { /\s*(\d+) (\S+)/ ; (uc($2),"FLD$1") ;} split("\n",$flds{"Fields"}) ;
	    my @pars = keys %pars ;
	    while(@pars) {
		my $p = shift @pars ;
		if(exists $fldtrans{$p}) {
		    $pars{$fldtrans{$p}} = $pars{$p} ;
		    delete $pars{$p} ;
		}
	    }
	    $pars{"LIST"}="Y" ;
	} ;
	if(defined $url) {
	    my @pars = map { "$_=$pars{$_}" ; }  keys %pars ;
	    $shortcut_text .= &P4CGI::ahref(-url=>$url,
					    @pars,
					    $text) ;
	} ;
    }
    $shortcut_text .= $tmps;
    $SHORTCUTS = join("\n",(&P4CGI::start_table("align=center cellpadding=10 bgcolor=".&P4CGI::HDRFTR_BGCOLOR()),
			    &P4CGI::table_row($shortcut_text),
			    &P4CGI::end_table())) ;
} ;

if(&P4CGI::SHORTCUTS_ON_TOP()) {
    print "$SHORTCUTS",
} ;
    

my @MENU = (&P4CGI::ahref(-url => "depotTreeBrowser.cgi",
			  "Browse Depot"),
	    &P4CGI::ahref(-url => "changeList.cgi",
			  "FSPC=//...",
        "DATESPECIFIER=browse",
        "SHOWFILES=1",
			  "Submitted Changes"),
	    &P4CGI::ahref(-url => "changeList.cgi",
			  "FSPC=//...",
			  "STATUS=pending",
			  "Pending Changes"),

	    &P4CGI::ahref(-url => "fileOpen.cgi",
			  "Open files"),
	    &P4CGI::ahref(-url => "branchList.cgi",
			  "Branches"),
	    &P4CGI::ahref(-url => "labelList.cgi",
			  "Labels"),

	    &P4CGI::ahref(-url => "jobList.cgi",
			  "Jobs"),
	    &P4CGI::ahref(-url => "userList.cgi",
			  "Users and Groups"),
	    &P4CGI::ahref(-url => "clientList.cgi",
			  "Clients"),

	    &P4CGI::ahref(-url => "changeByUsers.cgi",
			  "Changes by User or Group"),
	    &P4CGI::ahref(-url => "searchPattern.cgi",
			  "Search Changes by Descriptions"),
	    &P4CGI::ahref(-url => "filesChangedSince.cgi",
			  "List Recently Modified Files"),

	    &P4CGI::ahref(-url => "depotStats.cgi",
			  "Depot Statistics")
	    ) ;

if(uc(&P4CGI::USE_JAVA()) eq "YES") {
    push @MENU, ("<APPLET CODE=\"p4jdb/P4DirTreeApplet.class\" WIDTH=100 HEIGHT=30>\n".
		 "<param name=File value=\"javaDataView.cgi\">\n".
		 "</APPLET>") ;
} ;

my $COLS = 3 ;
print "",
    &P4CGI::start_table("width=100% cols=3 cellspacing=0 cellpadding=0") ;

my $colorCnt=0 ;
while(@MENU > 0) {
    my $n ;
    my @alts ;
    for($n = 0;($n < $COLS) and (@MENU > 0);$n++) {
	my $t =  shift @MENU ;
	push @alts, "<font size=+1>$t</font>"; 
    } ;
    my $tmp = @alts ;
    my @color = (&P4CGI::BGCOLOR(),&P4CGI::HDRFTR_BGCOLOR()) ;
    print &P4CGI::table_row(-align => "center",
			    map { 
				  {-width => "33%",
				   -bgcolor => $color[$colorCnt++ & 1],
				   -text => "$_" } ; } @alts) ."\n" ;
} ;   

print &P4CGI::end_table() ;

if(!&P4CGI::SHORTCUTS_ON_TOP()) {
    print "$SHORTCUTS\n"
} ;
    


print "",
    &P4CGI::start_table("bgcolor=".&P4CGI::HDRFTR_BGCOLOR()." align=center cellpadding=0 cellspacing=2"),
    "<tr><td>\n" ;


sub prSelection($$$$ )
{
    my $cgitarget  = shift @_ ;
    my $desc       = shift @_ ;
    my $fields     = shift @_ ;
    my $helpTarget = shift @_ ;
    
    print "", &P4CGI::table_row(-valign=>"center",
				{-align=>"center",
				 -text => 
				     join("\n",
					  &P4CGI::cgi()->startform(-action => $cgitarget,
								   -method => "GET"),
					  "<font size=+1>$desc</font>")},
				{-align=>"left",
				 -text => $fields},
				{-align=>"left",
				 -text => " "},
				{-align=>"left",
				 -valign=>"center",
				 -width=>"1",
				 -text =>  &P4CGI::cgi()->submit(-name  => "ignore",
								 -value => "GO!")
				 },
				{ -text => &P4CGI::cgi()->endform()
				  }
				) ;
} ;

print "",  &P4CGI::start_table("width=100% cellspacing=4") ;

my $limiter="<tr><td colspan=5><hr></td></tr>\n" ;

print $limiter ;

prSelection("changeList.cgi",
	    "List changes for<br>file spec",
	    join("","File spec:<font face=fixed>",
		 &P4CGI::cgi()->textfield(-name      => "FSPC",
					  -default   => "//...",
					  -size      => 50,
					  -maxlength => 256),
		 "</font>"),
	    "listCh") ;

print $limiter ;

prSelection("fileSearch.cgi",
	    "Search for file",
	    join("","File spec:<font face=fixed>",
		 &P4CGI::cgi()->textfield(-name      => "FSPC",
					  -default   => "//...",
					  -size      => 50,
					  -maxlength => 256),
		 "</font>"),
	    "fileSrch") ;

print $limiter ;

prSelection("changeView.cgi",
	    "View change",
	    join("","Change number:<font face=fixed>",
		 &P4CGI::cgi()->textfield(-name      => "CH",
					  -default   => "1",
					  -size      => 10,
					  -maxlength => 10),    
		 "</font>"),
	    "viewCh") ;

		
print &P4CGI::end_table() ;
		
print  "</tr></td>",&P4CGI::end_table() ;


print
    "<hr>",
    &P4CGI::start_table("width=100% cols=3"),
    &P4CGI::table_row(-align => "left",
		      &P4CGI::ahref(-url => &P4CGI::HELPFILE_PATH() . "/README.html",
				    "Readme file<br>for admin"),
		      {-align => "center",
		       -text =>  &P4CGI::ahref(-url => "SetPreferences.cgi",
					       "<FONT SIZE=+2>Set Preferences</FONT>"),
		      },
		      { -text => &P4CGI::ahref(-url => "p4race.cgi",
					       "<font size=-1>The Great<br>Submit Race</font>"),
			-align => "right" }),
    &P4CGI::end_table() ;

print "<hr>" ;

print    
    &P4CGI::end_page() ;

#
# That's all folks
#




