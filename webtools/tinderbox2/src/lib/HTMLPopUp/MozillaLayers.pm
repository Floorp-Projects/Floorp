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



# $Revision: 1.17 $ 
# $Date: 2003/08/17 01:31:50 $ 
# $Author: kestes%walrus.com $ 
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

# complete rewrite by Ken Estes for contact info see the
#     mozilla/webtools/tinderbox2/Contact file.
# Contributor(s): 



package HTMLPopUp::MozillaLayers;

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
<html>
        <!-- This file was automatically created by $main::0  -->
        <!-- version: $main::VERSION -->
        <!-- at $main::LOCALTIME -->
<head>
	$refresh
<script>

var tipRef=null;
var fixedLayer=null;

function tip(w,h,c) {
	

	var divX=lastX;
	var divY=lastY;

	if(navigator.userAgent.indexOf("Gecko")>-1) {
		if((lastX+w)>window.innerWidth) {
			divX-=((lastX+w)-window.innerWidth);
		}
		if((lastY+h)>window.innerHeight) {
			divY-=((lastY+h)-window.innerHeight);
		}
		divPanel=document.createElement("div");
		divPanel.style.position="absolute";
		divPanel.style.left=(divX-10)+"px";
		divPanel.style.top=(divY-10)+"px";
		divPanel.style.height=h+"px";
		divPanel.style.width=w+"px";
		divPanel.style.backgroundColor="white";
		document.body.appendChild(divPanel);


		//window.onmousedown=hideTip;	
		divPanel.innerHTML=c;
		tipRef=divPanel;

	} else if(document.all) {

		if((lastX+w)>document.body.scrollWidth) {
			divX-=((lastX+w)-document.body.scrollWidth);
		}
		if((lastY+h)>document.body.scrollHeight) {
			divY-=((lastY+h)-document.body.scrollHeight);
		}
		divPanel=document.createElement("div");
		divPanel.style.position="absolute";
		divPanel.style.left=(divX-10)+"px";
		divPanel.style.top=(divY-10)+"px";
		divPanel.style.height=h+"px";
		divPanel.style.width=w+"px";
		divPanel.style.backgroundColor="white";
		document.body.appendChild(divPanel);
		document.onmousedown=hideTip;	
		divPanel.innerHTML=c;
		tipRef=divPanel;

	} else if(document.layers) {

		if((lastX+w)>window.innerWidth) {
			divX-=((lastX+w)-window.innerWidth);
		}
		if((lastY+h)>window.innerHeight) {
			divY-=((lastY+h)-window.innerHeight);
		}
		divPanel=fixedLayer;
		divPanel.position="absolute";
		divPanel.left=(divX-10);
		divPanel.top=(divY-10);
		divPanel.clip.height=h;
		divPanel.clip.width=w;
		divPanel.width=w;
		divPanel.height=h;
		divPanel.bgColor="white";
		divPanel.visibility="show";

		document.onmousedown=hideTip;	

		divPanel.document.open("text/html");
		divPanel.document.write(c);
		divPanel.document.close();

		tipRef=divPanel;

	}

}

function hideTip(e) {
	if(navigator.userAgent.indexOf("Gecko")>-1) {
		document.body.removeChild(divPanel);
		window.onmousedown=null;
	}
	if(document.all) {
		document.body.removeChild(divPanel);
		document.onmousedown=null;
	}

	if(document.layers) {
		fixedLayer.visibility="hide";
		document.onmousedown=null;
	}
}


function addEvents() {
	if(navigator.userAgent.indexOf("Gecko")>-1) {
		window.onmousemove=logMouse;
	}
	if(document.all) {
		document.onmousemove=logMouse;
	}
	if(document.layers) {
		document.captureEvents(Event.MOUSEMOVE|Event.MOUSEDOWN);
		document.onmousemove=logMouse;
		fixedLayer=new Layer(300);

	}
}

var lastY=0;
var lastX=0;

function logMouse(e) {
	if(navigator.userAgent.indexOf("Gecko")>-1) {
		lastX=e.clientX;
		lastY=e.clientY;
	}

	if(document.layers) {
		lastX=e.pageX;
		lastY=e.pageY;
	}

	if(document.all) {
		lastX=event.clientX;
		lastY=event.clientY;
	}


}

function start() {
	addEvents();
}

</script>
        <TITLE>$args{'title'}</TITLE>
</head>
<body onload="start()" >

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

  my $name ="";

  if ($args{'name'}) {
    $name = "name=\"$args{'name'}\"";
  }

  # This is important to prevent recursion.  Our popup windows have
  # links inside them.  Luckally the links do not cause popups.

  if (!($args{'windowtxt'})) {
      $out .= "<a $name href=\"$args{'href'}\">";
      $out .= "$args{'linktxt'}</a>\n";
      
      return $out;
  }

  if (! scalar(@POPUPTXT)) {
    # Make the first item blank so we can shut down the windows.
    push @POPUPTXT, '';
  }


  $out .= "<A $name HREF=\"javascript:\" ";

  if (($args{'statuslinetxt'}) || ($args{'windowtxt'})) {

    # set the defaults

    $args{'windowtitle'} = $args{'windowtitle'} || 
      $HTMLPopUp::DEFAULT_POPUP_TITLE;


    # the arguments:

    #	$args{'windowheight'}
    #	$args{'windowwidth'}

    # were a hack needed for another implementation.  We ignore them
    # as the window will be sized properly without their help.



    # perhaps we should not allow the interface to determine the width
    # but we should determine it ourselves based on longest_line2width
    # number_of_lines2hight conversion factors, but it is hard to
    # determine what a HTML row is.

    $args{'windowheight'} = ($args{'windowheight'} || 
                             $HTMLPopUp::DEFAULT_POPUP_HEIGHT);

    $args{'windowwidth'} = ($args{'windowwidth'} ||
                            $HTMLPopUp::DEFAULT_POPUP_WIDTH);


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
    


    $out .= "onClick=\" ";
    ($args{'windowtxt'}) &&
      ($out .= "tip(".(
#                        "$args{'windowheight'},".
#                        "$args{'windowwidth'},".
                       "$HTMLPopUp::DEFAULT_POPUP_HEIGHT,".
                       "$HTMLPopUp::DEFAULT_POPUP_WIDTH,".
                       "\'$args{'windowtxt'}\'".
                      "").
       "); ");
    $out .= "return false\" ";


  }

  $out .= ">$args{'linktxt'}</a>";

  return $out;
}


# After all the links have been rendered we may need to dump some
# static data structures into the top of the HTML file. 


sub define_structures {
  my $self = shift @_;
  my (@out) =();

  push @out, "\t<SCRIPT>\n";
  foreach $i (0 .. $#POPUPTXT) {
#    push @out, "\t\tlogtxt$i = \"$POPUPTXT[$i]\"\;\n";
  }
  push @out, "\t</SCRIPT>\n\n";

  return @out;
}

1;

