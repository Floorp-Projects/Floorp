# -*- Mode: perl; indent-tabs-mode: nil -*-

# HTMLPopUp::MozillaLayers.pm - An implementation of popup windows using
# javascript and taken from pages returned by mozilla.org's bonsai
# application.

#  This PopUp window code was taken from the the 
#  bonsai tool developed at netscape. 
#
#     http://bonsai.mozilla.org

# I have had trouble with it on non netscape browsers but it has some
# advantages over other other popup methods which are included in
# tinderbox.



# $Revision: 1.5 $ 
# $Date: 2001/03/26 14:03:14 $ 
# $Author: kestes%tradinglinx.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/HTMLPopUp/MozillaLayers.pm,v $ 
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

# complete rewrite by Ken Estes:
#	 kestes@staff.mail.com Old work.
#	 kestes@tradinglinx.com New work.
#	 kestes@walrus.com Home.
# Contributor(s): 


# We need this empty package namespace for our dependency analysis, it
# gets confused if there is not a package name which matches the file
# name and in this case the file is one of several possible
# implementations.

package HTMLPopUp::MozillaLayers;

package HTMLPopUp;

$VERSION = '#tinder_version#';



# call like this
#    page_header(
#                'title'=>""
#                'refresh'=>""
#               );


sub page_header {
  my (%args) = @_;

  my ($html_time) = $main::LOCALTIME;
  $html_time =~ s/:[^:]+$//;
  
  my ($header) = '';

  my ($refresh) = '';

  ($args{'refresh'}) &&
    ( $refresh =  "<META HTTP-EQUIV=\"Refresh: $args{'refresh'}\" CONTENT=\"300\">" );
  
$header .=<<EOF;
<HTML>
        <!-- This file was automatically created by $main::0  -->
        <!-- version: $main::VERSION -->
        <!-- at $main::LOCALTIME -->
<HEAD>
	$refresh
	<SCRIPT>
var event = 0;	// Nav3.0 compatibility
document.loaded = false;

popup_offset = 5;
max_link_length = 0;

initialLayer = "<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=3><TR><TD BGCOLOR=#F0A000><TABLE BORDER=0 CELLSPACING=0 CELLPADDING=6><TR><TD BGCOLOR=#FFFFFF><B>Page loading...please wait.</B></TD></TR></TABLE></td></tr></table>";


function finishedLoad() {
    if ( (parseInt(navigator.appVersion) < 4) ||
	 (navigator.userAgent.toLowerCase().indexOf("msie") != -1) ) {
      return true;
    }
    document.loaded = true;
    document.layers['popup'].visibility='hide';
    
    return true;
}


function log(event, rev) {
    if ( (parseInt(navigator.appVersion) < 4) ||
	 (navigator.userAgent.toLowerCase().indexOf("msie") != -1) ) {
      return true;
    }

    var l = document.layers['popup'];
    var guide = document.layers['popup_guide'];

    if (event.target.text.length > max_link_length) {
      max_link_length = event.target.text.length;

      guide.document.write("<PRE>" + event.target.text);
      guide.document.close();

      popup_offset = guide.clip.width;
    }

    if (document.loaded) {
        l.document.write("<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=3><TR><TD BGCOLOR=#F0A000>");
        l.document.write("<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=6><TR><TD BGCOLOR=#FFFFFF><tt>");
        l.document.write(eval("logtxt" + rev));
        l.document.write("</TD></TR></TABLE>");
	l.document.write("</td></tr></table>");
        l.document.close();
    }

    if (event.target.y > (window.innerHeight + window.pageYOffset - l.clip.height) ) { 
         l.top = (window.innerHeight + window.pageYOffset - l.clip.height) - 15;
    } else {
         l.top = event.target.y - 9;
    }
    l.left = event.target.x + popup_offset;

    l.visibility="show";

    return true;
}

</SCRIPT>
        <TITLE>$args{'title'}</TITLE>
</HEAD>

<BODY onLoad="finishedLoad();" BGCOLOR="#FFFFFF" TEXT="#000000" LINK="#0000EE" VLINK="#551A8B" ALINK="#F0A000">
<LAYER SRC="javascript:initialLayer" NAME='popup' onMouseOut="this.visibility='hide';" LEFT=0 TOP=0 BGCOLOR='#FFFFFF' VISIBILITY='hide'></LAYER>
<LAYER SRC="javascript:initialLayer" NAME='popup_guide' onMouseOut="this.visibility='hide';" LEFT=0 TOP=0 VISIBILITY='hide'></LAYER>

<TABLE BORDER=0 CELLPADDING=12 CELLSPACING=0 WIDTH=\"100%\">
<TR><TD>
   <TABLE BORDER=0 CELLPADDING=0 CELLSPACING=2>
      <TR><TD VALIGN=TOP ALIGN=CENTER NOWRAP>
           <FONT SIZE=\"+3\"><B><NOBR>$args{'title'}</NOBR></B></FONT>
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



# There is a limitation in this popup imlementation.  You can not have
# quote characters or carrage returns.  Yuck! This limits the HTML you
# can put in the linktxt.

sub Link {

# call the function like this 
#
# Link(
#	  "statuslinetxt"=>"", 
#	  "windowtxt"=>"", 
#	  "linktxt"=>"", 
#	  "name"=>"", 
#	  "href"=>"",
#
# (arguments with defaults)
#
#	  "windowtitle"=>"", 
#	  "windowheight"=>"", 
#	  "windowwidth"=>"",
#	 );

  my (%args) = @_;
  my $out = '';

  my $name ="";

  if ($args{'name'}) {
    $name = "name=\"$args{'name'}\"";
  }



  if (! scalar(@POPUPTXT)) {
    # Make the first item blank so we can shut down the windows.
    push @POPUPTXT, '';
  }


  $out .= "<A $name HREF=\"$args{'href'}\" ";

  if (($args{'statuslinetxt'}) || ($args{'windowtxt'})) {

    # set the defaults

    $args{'windowtitle'} = $args{'windowtitle'} || 
      $HTML::DEFAULT_POPUP_TITLE;


    # the arguments:

    #	$args{'windowheight'}
    #	$args{'windowwidth'}

    # were a hack needed for another implementation.  We ignore them
    # as the window will be sized properly without their help.



    
    if ($args{'windowtxt'}) {

      # These characters inside the popupwindow will confuse my popup
      # window software into thinking the arguments are over.

      $args{'windowtxt'} =~ s/[\n\'\"]//g;
      $args{'windowtxt'} =~ s/[\n\t]+/ /g;
      $args{'windowtxt'} =~ s/\s+/ /g;

      # we do not have a proper place for a title so put it in the
      # window contents.

      ($args{'windowtitle'}) &&
        ($args{'windowtxt'} = ("<h3>$args{'windowtitle'}</h3>".
                               "$args{'windowtxt'}"));

      push @POPUPTXT, $args{'windowtxt'};
    }
    


    $out .= "onMouseOver=\" ";
    ($args{'windowtxt'}) &&
      ($out .= "log(".(
                       "event,".
                       "\'$#POPUPTXT\'".
                      "").
       "); ");
    $out .= "return true\" ";

    # It is safer to define some null action for the events to ensure
    # the window is shut down on mouse out and click, but this leaves
    # a strange box on the screen.


    $out .= "onClick=\" ";
    ($args{'windowtxt'}) &&
      ($out .= "log(".(
                       "event,".
                       "\'0\'".
                      "").
       "); ");
    $out .= "return true\" ";

    $out .= "onMouseOut=\" ";
    ($args{'windowtxt'}) &&
      ($out .= "log(".(
                       "event,".
                       "\'0\'".
                      "").
       "); ");
    $out .= "return true\" ";

  }

  $out .= ">$args{'linktxt'}</a>";

  return $out;
}


# After all the links have been rendered we may need to dump some
# static data structures into the top of the HTML file. 


sub define_structures {
  my (@out) =();

  push @out, "\t<SCRIPT>\n";
  foreach $i (0 .. $#POPUPTXT) {
    push @out, "\t\tlogtxt$i = \"$POPUPTXT[$i]\"\;\n";
  }
  push @out, "\t</SCRIPT>\n\n";

  return @out;
}

1;

