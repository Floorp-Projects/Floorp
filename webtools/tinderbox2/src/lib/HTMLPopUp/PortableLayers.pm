# -*- Mode: perl; indent-tabs-mode: nil -*-

# HTMLPopUp::PortableLayers.pm - A portable implementation of popup
# windows using javascript layers written by Elena Dykhno.


# $Revision: 1.6 $ 
# $Date: 2003/08/17 01:31:50 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/HTMLPopUp/PortableLayers.pm,v $ 
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


package HTMLPopUp::PortableLayers;

$VERSION = '#tinder_version#';



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

<style type="text/css">
#divPopup
         {
         position:absolute;
         width:200;
         visibility:hidden;
         z-index:200;
         }
.popupBody
         {
         width:300;
         font-family:verdana,arial;
         font-size:11px;
         background-color:#99cccc;
         padding:20px;
         border: 1px solid #3399cc;
         }
.popupBodyT
         {
         font-family:verdana,arial;
         font-size:11px;
         background-color:#cc9933;
         padding:18px;
         border: 1px solid #3399cc;
         }

#divLinks
         {
         position:absolute;
         z-index:1;
         }
</style>

<script language="JavaScript">

var isInit;
var x,y;
x = 5;
y = 5;

function whichAgent()
     {
     this.version = navigator.appVersion;
     this.dom = document.getElementById?1:0;
     this.ie5 = (this.version.indexOf("MSIE 5")>-1 && this.dom)?1:0;
     this.ie4 = (document.all && !this.dom)?1:0;
     this.ns5 = (this.dom && parseInt(this.version) >= 5)?1:0;
     this.ns4 = (document.layers && !this.dom)?1:0;
     this.browser = (this.ie5 || this.ie4 || this.ns5 || this.ns4)
     return this;
     }
browser  =   new whichAgent();

function newWindow(pop)
     {
     this.css = 
browser.dom?document.getElementById(pop).style:browser.ie4?document.all[pop].style:browser.ns4?document.layers[pop]:0;
     this.win = 
browser.dom?document.getElementById(pop):browser.ie4?document.all[pop]:browser.ns4?document.layers[pop].document:0;
     this.writePopup = popUp;
     return this;
     }

function popUp(txt)
         {
         if(browser.ns4)
             {
             this.win.write(txt);
             this.win.close();
             }
        else
             {
             this.win.innerHTML = txt;
             }
         }
function move(evt)
     {
         x = browser.ns4?evt.pageX:evt.x;
         y = browser.ns4?evt.pageY:evt.y;
     }

function popupInit()
     {
     viewPopup = new newWindow('divPopup');
     if(browser.ns4)document.captureEvents(Event.MOUSEMOVE);
     document.onmousemove = move;
     isInit = true;
     }

function popup(index, body)
     {
     if(isInit)
         {
         viewPopup.writePopup('<span class="popupBody">'+body+'</span>');
         if(browser.ie5)
             y = y+document.body.scrollTop;
             viewPopup.css.left=x+5;
             viewPopup.css.top=y+5;
         viewPopup.css.visibility='visible'
         }
     }
function popdown(index)
     {
     if(isInit)
     viewPopup.css.visibility='hidden';
     }


onload = popupInit;

</script>

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


<div id="divPopup"></div>


<div id="divLinks">


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

  my $name ="";

  if ($args{'name'}) {
    $name = "name=\"$args{'name'}\"";
  }


#  if (! scalar(@POPUPTXT)) {
#   # the first item must be blank so we can shut down the windows.
#    push @POPUPTXT, '';
#  }

  $out .= "<A $name HREF=\"$args{'href'}\" ";

  if (($args{'statuslinetxt'}) || ($args{'windowtxt'})) {

    # set the defaults

    $args{'windowtitle'} = $args{'windowtitle'} || 
      $HTMLPopUp::DEFAULT_POPUP_TITLE;


    # These characters inside the popupwindow will confuse my popup
    # window software into thinking the arguments are over.

    if ($args{'windowtxt'}) {
      $args{'windowtxt'} =~ s/[\n\'\"]//g;
      $args{'windowtxt'} =~ s/[\n\t]+/ /g;
      $args{'windowtxt'} =~ s/\s+/ /g;

    }

    if ($args{'windowtxt'}) {
	# COUNTER of '' is 0
	( $COUNTER < 1 ) && ( $COUNTER = 0 );
	$out .= (
		 "onMouseOver=\"popup($COUNTER,\'$args{'windowtxt'}\')\" ".
		 "onMouseOut=\"popdown($COUNTER)\" "
		 ); 
	$COUNTER++;
    }

  }

  $out .= ">$args{'linktxt'}</A>";

  return $out;
}


# After all the links have been rendered we may need to dump some
# static data structures into the top of the HTML file. 


sub define_structures {
    my $self = shift @_;

    my $out = "</div>\n\n\n";

    return $out;
}

1;

