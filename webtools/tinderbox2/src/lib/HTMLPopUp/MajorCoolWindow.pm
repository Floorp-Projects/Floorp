# -*- Mode: perl; indent-tabs-mode: nil -*-

# HTMLPopUp::MajorCoolWindow.pm - A portable implementation of
# popup windows using javascript and taken from a library in the
# application MajorCool.


# $Revision: 1.14 $ 
# $Date: 2003/08/17 01:31:49 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/HTMLPopUp/MajorCoolWindow.pm,v $ 
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


package HTMLPopUp::MajorCoolWindow;

$VERSION = '#tinder_version#';



#        // This PopUp window code was taken from the the 
#        // Major Cool index HTML page Vesion 1.3.0 
#        // http://ncrinfo.ncr.com/pub/contrib/unix/MajorCool/

#
#        // It is written up at http://cyberpass.net/~bhoule/WebWorks/
#        // The only license I can find for it says:
#        //         Feel free to steal the code. 

# I removed the image handling code, I am not using it.
# I removed the style sheet as we need fixed width fonts
# I moved some global variables into more arguments to tip()


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
        <SCRIPT LANGUAGE="JavaScript">

        <!-- Hide Script from Ill-Behaved Browsers

        //
        // This PopUp window code was taken from the the 
        // Major Cool index HTML page Vesion 1.3.0 
        // http://ncrinfo.ncr.com/pub/contrib/unix/MajorCool/

        // It is written up at http://cyberpass.net/~bhoule/WebWorks/
        // The only license I can find for it says
        //         Feel free to steal the code. 
        //
	// for Tinderbox:
	// I removed the image handling code, I am not using it.
	// I removed the style sheet as we need fixed width fonts
	// I moved some global variables into more arguments to tip()
	//
        //====================================================================
        // Active Images: dynamic mouseOver/mouseOut graphics.
        //
        //  o   You must explicitly set NAME=XXX in all <IMG> tags in
        //      order to take advantage of this feature.
        //
        //  o   Each image named XXX can have a complement grouping:
        //              XXX_normal -- the static image
        //              XXX_active -- the active image
        //              XXX_clicked-- the clicked image
        //
        //  o   Harmless to non-compliant browsers (eg, Win MSIE 3.x).
        //
        //--------------------------------------------------------------------
        // Bill Houle                           NCR Corporation
        // bhoule\@conveyanced.com               bill.houle\@sandiegoca.ncr.com
        //--------------------------------------------------------------------

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

        //--------------------------------------------------------------------
        // Tool Tip/Balloon Help: dynamic mouseOver/mouseOut windowing
        //
        //  o   Shows/hides text in pop-up window.
        //
        //  o   Delayed open to only pop those who pause for help.
        //
        //  o   mouseOut->mouseOver will re-use open window rather than
        //      than close & reopen.
        //
        //  o   Window closes after timeout. Timeout varies with amount
        //      of text displayed. More text, larger timeout.
        //
        //  o   Harmless to non-compliant browsers.
        //
        // Known bugs:
        //
        //   o  No control of window.open() placement.
        //
        //   o  In Win MSIE3.x: if tipWin is closed manually, the object
        //      is still valid but the window is not open. The timeout
        //      of the tip will result in 'scripting error 80010012'.
        //      Luckily, other browsers support onMouseOut, so the problem
        //      is not widespread.
        //      
        //--------------------------------------------------------------------
        // Bill Houle                           NCR Corporation
        // bhoule\@conveyanced.com               bill.houle\@sandiegoca.ncr.com
        //--------------------------------------------------------------------

        // Global settings; tweak as required
        //
        tipColor = "#FFFF80";                   // yellow background
        // Timing
        tipOpen = 150;                          // open msecs after mouseover
        tipClose = 800;                         // close msecs after mouseout
        tipFactor = 2;                          // est: chars read in 1 msec
        // Null defaults
        tipWin = null;
        tipOTime = null; tipOFlag = 0;
        tipCTime = null; tipCFlag = 0;

        //--------------------------------------------------------------------
        // tip(tipLocation, tipText, later)
        //      tipLocation -- handle:  a Window name
        //      tipText -- string:      the message to display in tipLocation
        //                              null string to close tipLocation
        //      later -- boolean:       show text immediately or after pause
        //
        // Usage:
        //      onMouseOver='tip(win,"This is descriptive.","title",150,150,1); return true;'
        //      onMouseOut='tip(win,"","",0,0,0); return true;'
        //      onClick='tip(win,"","",0,0,0); return true;'
        //
        //   o  Don\'t forget the onClick(), or else the tip window may pop
        //      up while you are off doing whatever the click action was.
        //
        function tip(tipLocation, tipTitle, tipText, tipHeight, tipWidth, later) {
                //
                // Clear any pending window opens or closes. Please note:
                //   uninitialized "timeout id" in NN3.x is untestable 'opaque'
                //   object, while "timeout id" in MSIE *must* be tested before
                //   clearTimeout(). Therefore, use global integers to bypass
                //   this Catch-22 situation.
                //
                if (tipOFlag != 0) { clearTimeout(tipOTime); tipOFlag = 0; }
                if (tipCFlag != 0) { clearTimeout(tipCTime); tipCFlag = 0; }
                //
                // Remove the tip from display
                //

                if (tipText == '') {
                        if (tipWin == null) return;     // already gone
                        //
                        // Close window in the future (extra delay is to
                        //   avoid popping down & up if there is a subsequent
                        //   mouseOver).
                        //
                        tipCTime = window.setTimeout(
                                "if (tipWin != null) {tipWin.close(); tipWin=null;}",
                                tipClose);
                        tipCFlag = 1;
                }
                //
                // Display the tip
                //
                else {
                        //
                        // If not yet open and requesting delayed open...
                        //
                        if (tipWin == null && later > 0) {
                                tipOTime = window.setTimeout(
                                        "tip('"+tipLocation+"','"+tipTitle+"','"+tipText+"',"+tipHeight+","+tipWidth+",0);",
                                        tipOpen);
                                tipOFlag = 1;
                                return;
                        }
                        //                        // MSIE requires explicit options on open(). Also,
                        //   contrary to appearances, this double open is not
                        //   for the known NN2 open() bug (which ignores the
                        //   location URL), but rather for an IE3 problem that
                        //   creates a window object without a valid document
                        //   object within.
                        // The 'dependent' attribute is Netscapism to indicate
                        //   a window child relationship.
                        //
                        tipWin = window.open("",tipLocation,
                                "width="+tipWidth+",height="+tipHeight+","+
                                "toolbar=0,menubar=0,scrollbars=0,"+
                                "location=0,status=0,resizable=0,"+
                                "directories=0,dependent=1");
                        tipWin = window.open("",tipLocation,
                                "width="+tipWidth+",height="+tipHeight+","+
                                "toolbar=0,menubar=0,scrollbars=0,"+
                                "location=0,status=0,resizable=0,"+
                                "directories=0,dependent=1");
                        if (tipWin == null) return;     // whoops!
                        // bring window to front
                        if (parseInt(navigator.appVersion) > 2) tipWin.focus();
                        if (tipWin.document) {
                                tipWin.document.open('text/html');
                                tipWin.document.write("<HEAD><TITLE>");
                                tipWin.document.write(tipTitle);
                                tipWin.document.write("</TITLE></HEAD>");
                                tipWin.document.write("<BODY BGCOLOR='"+tipColor+"'>");
                                tipWin.document.write("<P><FONT SIZE=4>");
                                tipWin.document.write(tipText.bold());
                                tipWin.document.write("</FONT></BODY>");
                                tipWin.document.close();
                        }
                        else if (tipWin) {
                                // open window, but still no document?!?
                                tipWin.close();
                                return;
                        }
                        //
                        // Estimate how long it takes to read this much text;
                        //   blank the tip text after a suitable delay for
                        //   reading.
                        //
                        tipRead = Math.round(tipText.length/tipFactor) * 1000;
                        tipOTime = window.setTimeout(
                                "tip('"+tipLocation+"','','',0,0,0);",
                                tipRead);
                        tipOFlag = 1;
                }
        }

        //====================================================================
        // End of JavaScript -->
        

</SCRIPT>
        <TITLE>$args{'title'}</TITLE>
        </HEAD>
        <BODY TEXT="#000000" BGCOLOR="#ffffff">


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

      push @POPUPTXT, $args{'windowtxt'};

    }

    # perhaps we should not allow the interface to determine the width
    # but we should determine it ourselves based on longest_line2width
    # number_of_lines2hight conversion factors, but it is hard to
    # determine what a HTML row is.

    $args{'windowheight'} = ($args{'windowheight'} || 
                             $HTMLPopUp::DEFAULT_POPUP_HEIGHT);

    $args{'windowwidth'} = ($args{'windowwidth'} ||
                            $HTMLPopUp::DEFAULT_POPUP_WIDTH);

    #

    $popup .= "onClick=\" ";
    ($args{'statuslinetxt'}) &&
      ($popup .= "msg(\'$args{'statuslinetxt'}\'); ");
    ($args{'windowtxt'}) &&
      ($popup .= "tip(".(
                       "\'TOP\',".
                       "\'$args{'windowtitle'}\',".
                       "popuptxt[$#POPUPTXT],".
                       "$args{'windowheight'},".
                       "$args{'windowwidth'},".
                       "1".
                      "").
       "); ");
    $popup .= "return true;\" ";
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
  my $self = shift @_;
  my (@out) =();

  push @out, "<script language=\"JavaScript\">\n";
  push @out, "// Separate out the popup window string table\n";
  push @out, "// in order to make the structure of the HTML easier to read.\n";
  push @out, "popuptxt = new Array();\n";
  foreach $i (0 .. $#POPUPTXT) {
      push @out, "popuptxt[$i] = \"$POPUPTXT[$i]\"\;\n";
  }
  push @out, "\n";
  push @out, "</script>\n";

  return @out;
}

1;

