# -*- Mode: perl; indent-tabs-mode: nil -*-

# HTMLPopUp::OverLib.pm - A portable implementation of
# popup windows using javascript and taken from a library in the
# application http://www.bosrup.com/web/overlib/ 
# OverLib HTML page Vesion 3.51.
# Contributed by dominik.stadler@gmx.at 


# $Revision: 1.1 $
# $Date: 2003/01/22 15:44:54 $
# $Author: kestes%walrus.com $
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/HTMLPopUp/OverLib.pm,v $
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
#	 kestes@reefedge.com New work.
#	 kestes@walrus.com Home.
# Contributor(s): dominik.stadler@gmx.at 


package HTMLPopUp::OverLib;

$VERSION = '#tinder_version#';

$SRC="/overlib_mini.js".

#        // This PopUp window code was taken from the the
#        // OverLib HTML page Vesion 3.51
#        // http://www.bosrup.com/web/overlib/


# call like this
#    page_header(
#                'title'=>""
#                'refresh'=>""
#               );


sub page_header {
  my $self = shift @_;
  my (%args) = @_;

  my ($html_time) = $main::LOCALTIME;
  $html_time =~ s/:[^:]+$//;

  my ($header) = '';

  my ($refresh) = '';

  ($args{'refresh'}) &&
    ( $refresh =  "<META HTTP-EQUIV=\"Refresh\" CONTENT=\"$args{'refresh'}\">" );

$header .=<<EOF;
<HTML>
        <!-- This file was automatically created by $main::0  -->
        <!-- version: $main::VERSION -->
        <!-- at $main::LOCALTIME -->
<HEAD>
	$refresh
<script language=\"JavaScript\" src=\"$SRC\">
<!-- overLIB (c) Erik Bosrup --></script>

<script language=\"JavaScript\">
        //====================================================================

        //--------------------------------------------------------------------
        // msg(msgText)
        //      msgText -- string:      text to display on browser Status line
        //                              null string to clear Status
        //
        // Usage:
        //      onMouseOver='msg("Hit me!"); return true;'
        //      onMouseOut='msg("Maybe next time?"); return true;'
        //      onClick='msg("Ouch!"); return true;'
        //
        //
        function msg(msgText) {
                window.status = msgText;
        }

        //====================================================================
</script>

        <TITLE>$args{'title'}</TITLE>
        </HEAD>
        <BODY TEXT=\"#000000\" BGCOLOR=\"#ffffff\">

<script language=\"JavaScript\">
var ol_fgcolor = \"#FFFF80\";
//ol_capcolor = \"#ffcc00\";
//var ol_textfont = \"Courier New, Courier\";
var ol_textsize = \"2\";
//var ol_sticky = 1;
var ol_vauto = 1;
var ol_hauto = 1;
</script>

<div id=\"overDiv\" style=\"position:absolute; visibility:hidden; z-index:1000;\"></div>
<script language=\"JavaScript\" src=\"$SRC\">
<!-- overLIB (c) Erik Bosrup --></script>

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
#         "sticky"=>"",
#
# (arguments with defaults)
#
#	  "windowtitle"=>"",
#	  "windowheight"=>"",
#	  "windowwidth"=>"",
#         "sticky"=>"",
#	 );

  my $self = shift @_;
  my (%args) = @_;
  my $out = '';

#  if (! scalar(@POPUPTXT)) {
#   # the first item must be blank so we can shut down the windows.
#    push @POPUPTXT, '';
#  }

  my $popup = '';

  if (($args{'statuslinetxt'}) || ($args{'windowtxt'})) {

    # set the defaults

    $args{'windowtitle'} = $args{'windowtitle'} ||
      $DEFAULT_POPUP_TITLE;


    # These characters inside the popupwindow will confuse my popup
    # window software into thinking the arguments are over.

    if ($args{'windowtxt'}) {
      $args{'windowtxt'} =~ s/[\n\'\"]//g;
      $args{'windowtxt'} =~ s/[\n\t]+/ /g;
      $args{'windowtxt'} =~ s/\s+/ /g;

#    push @POPUPTXT, $args{'windowtxt'};

    }

    # perhaps we should not allow the interface to determine the width
    # but we should determine it ourselves based on longest_line2width
    # number_of_lines2hight conversion factors, but it is hard to
    # determine what a HTML row is.

#    $args{'windowheight'} = ($args{'windowheight'} ||
#                             $HTMLPopUp::DEFAULT_POPUP_HEIGHT);

#    $args{'windowwidth'} = ($args{'windowwidth'} ||
#                            $HTMLPopUp::DEFAULT_POPUP_WIDTH);

    #


	# overlib can be used like this:
	# onmouseover="return overlib('HTMLText');" onmouseout="return nd();"


    $popup .= "onMouseOver=\" ";
    ($args{'statuslinetxt'}) &&
      ($popup .= "msg(\'$args{'statuslinetxt'}\'); ");

    if($args{'windowtxt'})
    {
       $popup .= "return overlib(\'$args{'windowtxt'}\'";

       if($args{'windowtitle'})
       {
       	 $popup .= ",CAPTION,\'$args{'windowtitle'}\'";
       }
       if($args{'sticky'} eq "1" )
       {
         $popup .= ",STICKY";
       }


       $popup .= "); \" ";
    }

#      ($popup .= "tip(".(
#                       "\'TOP\',".
#                       "\'$args{'windowtitle'}\',".
#                       "\'$args{'windowtxt'}\',".
#                       "$args{'windowheight'},".
#                       "$args{'windowwidth'},".
#                       "1".
#                      "").
#       "); ");
#    $popup .= "return true\" ";



    $popup .= "onMouseOut=\" ";
    ($args{'statuslinetxt'}) &&
      ($popup .= "msg(''); ");
    ($args{'windowtxt'}) &&
      ($popup .= "return nd(); ");
    $popup .= "\" ";

#      ($popup .= "tip('TOP','','',0,0,0); ");
#    $popup .= "return true\" ";

#    $popup .= "onClick=\" ";
#    ($args{'statuslinetxt'}) &&
#      ($popup .= "msg(''); ");
#    ($args{'windowtxt'}) &&
#      ($popup .= "tip('TOP','','',0,0,0); ");
#    $popup .= "return true\"";

  }

  my $name = '';
  if ($args{'name'}) {
    $name = "name=\"$args{'name'}\"";
  }

  my $href = '';
  if ($args{'href'}) {
      $href = "HREF=\"$args{'href'}\"";
  }

  my $linktxt =  $args{'linktxt'} || '';

  $out .= "<A $name $href $popup>$linktxt</a>";

  return $out;
}


# After all the links have been rendered we may need to dump some
# static data structures into the top of the HTML file.

# ( Passing indexes to static structures should allow us to embed
#    quotes and new lines in our strings.
#    not yet used, attempts at utilization caused netscape to exit,
#    don't forget that tip() is recursive )

sub test_define_structures {
  my $self = shift @_;
  my (@out) =();

  push @out, "popuptxt = new Array();\n";
  foreach $i (0 .. $#POPUPTXT) {
    push @out, "popuptxt[$i] = \"$POPUPTXT[$i]\"\;\n";
  }
  push @out, "\n";

  return @out;
}

sub define_structures {
  return '';
}

1;

