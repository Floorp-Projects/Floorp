# -*- Mode: perl; indent-tabs-mode: nil -*-

# HTMLPopUp::MozillaClick.pm - # Made by the Netscape popu guru
# (mgalli@netscape.com) this library will look an behave very much
# like the original popups in Tinderbox1 however this should be
# portable to all browsers.


# $Revision: 1.4 $ 
# $Date: 2003/08/17 01:31:50 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/HTMLPopUp/MozillaClick.pm,v $ 
# $Name:  $ 



# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY` KIND, either express or
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


package HTMLPopUp::MozillaClick;

$VERSION = '#tinder_version#';





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
<HEAD>
	$refresh

        <!-- This file was automatically created by $main::0  -->
        <!-- version: $main::VERSION -->
        <!-- at $main::LOCALTIME -->

        <SCRIPT LANGUAGE="JavaScript">


		var tipRef=null;
		var fixedLayer=null;
  		// var tipColor="white";
  		var tipColor="#FFFF80\";

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
				divPanel.style.backgroundColor=tipColor;
				document.body.appendChild(divPanel);


				divPanel.innerHTML=c;
				tipRef=divPanel;

				divPanel.setAttribute("id","popup");

				divPanel.onmousedown=preventClickf;
				window.onmousedown=hideTip; 

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
				divPanel.style.backgroundColor=tipColor;
				document.body.appendChild(divPanel);
				divPanel.onmousedown=preventClickf;
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
				divPanel.bgColor=tipColor;
				divPanel.visibility="show";



				divPanel.document.open("text/html");
				divPanel.document.write(c);
				divPanel.document.close();

				/// disabled the following
				//divPanel.onload=setLayerCapture;

				document.onmousedown=hideTip;	
				tipRef=divPanel;

			}

		}


		function setLayerCapture() {
			/// 
			// not working.
			// but 4.x can still link through... we are just making the layer invisible.  
			fixedLayer.document.captureEvents(Event.MOUSEDOWN);
			fixedLayer.document.onmousedown=clickPreventf;
		}

		var preventClick=null;

		function hideTip(e) {

			if(!preventClick) {
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
			} else {
				preventClick=false;
			}
		}

		function preventClickf(e) {
			preventClick=true;
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

        </SCRIPT>
        <TITLE>$title</TITLE>
</HEAD>
<BODY ONLOAD="start()" TEXT="#000000" BGCOLOR="#ffffff">


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


    my $href = '';
    if ($args{'href'}) {
        $href .= "HREF=\"$args{'href'}\"";
    } elsif ($args{'windowtxt'}) {
        $href .= "HREF=\"javascript:void(0);\"";
    }

    my $linktxt =  $args{'linktxt'} || '';
    
    $args{'windowtitle'} = $args{'windowtitle'} || 
        $DEFAULT_POPUP_TITLE;
    
    # perhaps we should not allow the interface to determine the width
    # but we should determine it ourselves based on longest_line2width
    # number_of_lines2hight conversion factors, but it is hard to
    # determine what a HTML row is.

    $args{'windowheight'} = ($args{'windowheight'} || 
                             $HTMLPopUp::DEFAULT_POPUP_HEIGHT);

    $args{'windowwidth'} = ($args{'windowwidth'} ||
                            $HTMLPopUp::DEFAULT_POPUP_WIDTH);

    my $popup = '';


    if ($args{'windowtxt'}) {

        # These characters inside the popupwindow will confuse my
        # popup window software into thinking the arguments are
        # over. I believe this is a ntescape specific problem.

        $args{'windowtxt'} =~ s/[\n\'\"]//g;
        $args{'windowtxt'} =~ s/[\n\t]+/ /g;
        $args{'windowtxt'} =~ s/\s+/ /g;
        
        push @POPUPTXT, $args{'windowtxt'};
        
        $popup .= (
                   "onClick=\" ".
                   "tip(".(
                           "$args{'windowwidth'},".
                           "$args{'windowheight'},".
                           "popuptxt[$#POPUPTXT]".
                           "").
                   "); ".
                   "return false\" ".
                   "");
    }
    
    $out .= "<A $name $href $popup>$linktxt</A>";
    
    return $out;
}


# After all the links have been rendered we may need to dump some
# static data structures into the top of the HTML file.

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

