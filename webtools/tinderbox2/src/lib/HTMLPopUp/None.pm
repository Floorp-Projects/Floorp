# -*- Mode: perl; indent-tabs-mode: nil -*-

# HTMLPopUp::None.pm - the implementation of the header and link
# command which will be used if no popup menus are desired.

# $Revision: 1.10 $ 
# $Date: 2003/08/17 01:31:49 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/HTMLPopUp/None.pm,v $ 
# $Name:  $ 



# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#

# complete rewrite by Ken Estes for contact info see the
#     mozilla/webtools/tinderbox2/Contact file.
# Contributor(s): 



package HTMLPopUp::None;

$VERSION = '#tinder_version#';


# Each function here builds an $out string.  If there are bugs in the
# code you can put your breakpoint on the return statement and look at
# the completed string before it is returned.



# call like this
#    page_header(
#                'title'=>""
#                'refresh'=>""
#               );


sub page_header {
  my $self = shift @_;
  my (%args) = @_;

  my $title = $args{'title'} || '';

  my ($html_time) = $main::LOCALTIME;
  $html_time =~ s/:[^:]+$//;

  my ($header) = '';
  my ($refresh) = '';

  ($args{'refresh'}) &&
    ( $refresh =  "<META HTTP-EQUIV=\"Refresh\" CONTENT=\"$args{'refresh'}\">" );
  
$header .=<<EOF;
<HTML>
        <!-- This file was automatically created by $main::0  -->
        <!-- version: $main::$VERSION -->
        <!-- at $main::LOCALTIME -->
<HEAD>
	$refresh
        <TITLE>$title</TITLE>
        </HEAD>
        <BODY TEXT="#000000" BGCOLOR="#ffffff">


<TABLE BORDER=0 CELLPADDING=12 CELLSPACING=0 WIDTH=\"100%\">
<TR><TD>
   <TABLE BORDER=0 CELLPADDING=0 CELLSPACING=2>
      <TR><TD VALIGN=TOP ALIGN=CENTER NOWRAP>
           <FONT SIZE=\"+3\"><B><NOBR>$title</NOBR></B></FONT>
      </TD></TR>
      <TR><TD VALIGN=TOP ALIGN=CENTER>
           <B>Created at: $html_time</B>
      </TD></TR>
   </TABLE>
</TD></TR>
</TABLE>


EOF

  return $header;
}



# if we ever need to remove popups from the output just use this
# version of Link


# call the function like this 
#
# Link(
#	  "linktxt"=>"text to usually show ", 
#         "alt_linktxt" => Alternative text to display if there 
#                           is no popup library installed and the text 
#                           should be different from the normal linktxt
#	  "name"=>"so that  other links can point here", 
#	  "href"=>"where this link will go to",
#	  "windowtxt"=>"the contents of the popup window", 
#
# (These are deprecated: arguments with defaults)
#
#	  "windowtitle"=>"", 
#	  "windowheight"=>"", 
#	  "windowwidth"=>"",
#	 );


sub Link {
  my $self = shift @_;
  my (%args) = @_;
  my ($out) = '';
  
  my ($name) ="";

  if ($args{'name'}) {
    $name = "name=\"$args{'name'}\"";
  }

  $out .= "<A $name href=\"$args{'href'}\">";
  $out .= ($args{'alt_linktxt'}) || ($args{'linktxt'});
  $out .= "</A>\n";

  return $out;
}
  

# After all the links have been rendered we may need to dump some
# static data structures into the top of the HTML file.  Passing
# indexes to static structures should allow us to embed quotes and new
# lines in our strings.


sub define_structures {
  my $self = shift @_;
  my (@out) = ();

  return @out;
}

1;

