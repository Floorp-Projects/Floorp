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
#  Search changes for pattern
#
#################################################################

my $FSPC = &P4CGI::cgi()->param("FSPC") ;
$FSPC = "//..." unless defined $FSPC ;
&P4CGI::bail("Invalid file spec.") if ($FSPC =~ /[<>"&:;'`]/);

my @legend ;

my $legend = "" ;
$legend = &P4CGI::ul_list(@legend) if @legend > 0 ;

&P4CGI::SET_HELP_TARGET("searchPattern") ;

print "",  &P4CGI::start_page("Search Descriptions",$legend) ;

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
				 -valign=>"top",
				 -text => $fields},
				{-align=>"left",
				 -text => " "},
				{-align=>"left",
				 -valign=>"bottom",
				 -width=>"1",
				 -text =>  &P4CGI::cgi()->submit(-name  => "ignore",
								 -value => "GO!")
				 },
				{ -valign=>"bottom",
				  -text => &P4CGI::cgi()->endform()
				  },
				) ;
} ;

print "",  &P4CGI::start_table("width=100% cellspacing=4") ;


prSelection("changeList.cgi",
	    "Search for pattern<br>in change description",
	    join("\n",(&P4CGI::start_table(),
		       "<tr>",
		       "<td align=right valign=center>File spec:</td>",
		       "<td align=left valign=center><font face=fixed>",
		       &P4CGI::cgi()->textfield(-name      => "FSPC",
						-default   => "//...",
						-size      => 50,
						-maxlength => 256),		       
		       "</font></td></tr>",
		       "<td align=right valign=center>Pattern:</td>",
		       "<td align=left valign=center><font face=fixed>",
		       &P4CGI::cgi()->textfield(-name      => "SEARCHDESC",
						-default   => "<pattern>",
						-size      => 50,
						-maxlength => 256),
		       "</font></td></tr>",
		       "<td align=right valign=center>Invert search:</td>",
		       "<td align=left valign=center><font face=fixed>",
		       &P4CGI::cgi()->checkbox(-name    => "SEARCH_INVERT",
					       -value   => 1,
					       -label   => " Search descriptions <B>NOT</B> including pattern"),
		       "</font></td></tr>",
		       "</table>")),
	    "searchPatt") ;
		       
print &P4CGI::end_table() ;
		

print  "</tr></td>",&P4CGI::end_table() ;

print    
    &P4CGI::end_page() ;

#
# That's all folks
#




