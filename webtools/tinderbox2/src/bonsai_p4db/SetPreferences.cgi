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
#  Set Prefereces
#
#################################################################

my $newp = &P4CGI::cgi()->param("SET_PREFERENCES") ;
$newp = "Yes" if defined $newp;
my $fullURL = &P4CGI::cgi()->url(-full=>1) ;

if((defined $newp) and 
   (&P4CGI::CURRENT_CHANGE_LEVEL() >= 0)) {
    &P4CGI::EXTRAHEADER(-Refresh => "1; URL=index.cgi") ;
    
} ;
&P4CGI::ERRLOG("fullURL: $fullURL") ;
&P4CGI::ERRLOG("server_name: ". &P4CGI::cgi()->server_name()) ;

&P4CGI::SET_HELP_TARGET("SetPreferences") ;

print "",
    &P4CGI::start_page("Set Preferences","") ;    

my %pref_list=&P4CGI::PREF_LIST() ;
my %pref=&P4CGI::PREF() ;

print "",
    &P4CGI::cgi()->start_form(-method=>"GET",
			      -action=>"SetPreferences.cgi"),    
    &P4CGI::start_table("") ;
my $p ;
foreach $p (sort { $ {$pref_list{$a}}[0] cmp $ {$pref_list{$b}}[0] }  keys %pref_list) {
    my @data = @{$pref_list{$p}} ;
    my $type = shift @data ;
    $type =~ s/^\w:// ;
    my $desc = shift @data ;
    my $default = shift @data ;
    my $cval = $pref{$p} ;
    my $cell ;
    if($type eq "BOOL") {
	$cell = &P4CGI::cgi()->radio_group(-name=>$p,
					   "values"=>[0,1],
					   -default=> $cval,
					   -labels=>{1 => "Yes", 0 => "No"}) ;
    }
    if($type eq "LIST") {
	my $n = -1 ;
	my %alts = map { $n++ ; s/^.*;\s+// ; ($n,$_) ; } @data ;
	my @d = keys %alts ;
	$cell = &P4CGI::cgi()->popup_menu(-name=>$p,
					  "values"=>\@d,
					  -default=>$cval,
					  -labels=>\%alts) ;
    } 
    if($type eq "BGCOLOR") {
	my $n = -1 ;
	my %alts = map { $n++  ; ($n,"<TABLE BORDER BGCOLOR=\"$_\"><TR><TD>&nbsp;&nbsp;&nbsp;</TD></TR></TABLE>\n") } @data ;
#	my %alts = map { $n++  ; ($n,"$_") } @data ;
	my @d = keys %alts ;
	$cell = &P4CGI::cgi()->radio_group(-name=>$p,
					   "values"=>\@d,
					   -default=> $cval,
#					   -linebreak=>'true',
					   -labels=>\%alts,
					   -rows=>1) ;
    } 
    if($type eq "INT") {
	$cell = &P4CGI::cgi()->textfield(-name=>$p,
					 -size=>6,
					 -default=>$cval,
					 -maxlength=>6) ;
    } ;
    print &P4CGI::table_row({-type=>"th",
			     -text=>"$desc:",
			     -align=>"right"},
			    {
			     -align=>"left",
			     -text=>$cell}) ;
}
print &P4CGI::table_row("",&P4CGI::cgi()->submit(-value=>'Change preferences',
						 -name=>'SET_PREFERENCES') . " " .
			&P4CGI::cgi()->defaults("Reset")) ;

print "",&P4CGI::end_table(),"<HR>" ;

print    
    &P4CGI::end_page() ;

#
# That's all folks
#
