# -*- Mode: perl; indent-tabs-mode: nil -*-

# HTMLPopUp::OverLib.pm - A portable implementation of
# popup windows using javascript and taken from a library in the
# application http://www.bosrup.com/web/overlib/ 
# OverLib HTML page Vesion 3.51.
# Contributed by dominik.stadler@gmx.at 


# $Revision: 1.7 $
# $Date: 2006/09/24 15:48:27 $
# $Author: kestesisme%yahoo.com $
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

# complete rewrite by Ken Estes for contact info see the
#     mozilla/webtools/tinderbox2/Contact file.
# Contributor(s): dominik.stadler@gmx.at 


package HTMLPopUp::OverLib;

# Load standard perl libraries

# Load Tinderbox libraries

use lib '#tinder_libdir#';

use FileStructure;
use Utils;


$VERSION = '#tinder_version#';

# This PopUp window code was taken from the the
# OverLib HTML page Vesion 3.51
# http://www.bosrup.com/web/overlib/

# Notice that we need to use a single quote to keep it exactly as
# written.

$OVERLIB_JS .=<<'EOF';
//\/////
//\  overLIB 4.21 - You may not remove or change this notice.
//\  Copyright Erik Bosrup 1998-2004. All rights reserved.
//\
//\  Contributors are listed on the homepage.
//\  This file might be old, always check for the latest version at:
//\  http://www.bosrup.com/web/overlib/
//\
//\  Please read the license agreement (available through the link above)
//\  before using overLIB. Direct any licensing questions to erik@bosrup.com.
//\
//\  Do not sell this as your own work or remove this copyright notice. 
//\  For full details on copying or changing this script please read the
//\  license agreement at the link above. Please give credit on sites that
//\  use overLIB and submit changes of the script so other people can use
//\  them as well.
//   $Revision: 1.7 $                $Date: 2006/09/24 15:48:27 $
//\/////
//\mini

////////
// PRE-INIT
// Ignore these lines, configuration is below.
////////
var olLoaded = 0;var pmStart = 10000000; var pmUpper = 10001000; var pmCount = pmStart+1; var pmt=''; var pms = new Array(); var olInfo = new Info('4.21', 1);
var FREPLACE = 0; var FBEFORE = 1; var FAFTER = 2; var FALTERNATE = 3; var FCHAIN=4;
var olHideForm=0;  // parameter for hiding SELECT and ActiveX elements in IE5.5+ 
var olHautoFlag = 0;  // flags for over-riding VAUTO and HAUTO if corresponding
var olVautoFlag = 0;  // positioning commands are used on the command line
var hookPts = new Array(), postParse = new Array(), cmdLine = new Array(), runTime = new Array();
// for plugins
registerCommands('donothing,inarray,caparray,sticky,background,noclose,caption,left,right,center,offsetx,offsety,fgcolor,bgcolor,textcolor,capcolor,closecolor,width,border,cellpad,status,autostatus,autostatuscap,height,closetext,snapx,snapy,fixx,fixy,relx,rely,fgbackground,bgbackground,padx,pady,fullhtml,above,below,capicon,textfont,captionfont,closefont,textsize,captionsize,closesize,timeout,function,delay,hauto,vauto,closeclick,wrap,followmouse,mouseoff,closetitle,cssoff,compatmode,cssclass,fgclass,bgclass,textfontclass,captionfontclass,closefontclass');

////////
// DEFAULT CONFIGURATION
// Settings you want everywhere are set here. All of this can also be
// changed on your html page or through an overLIB call.
////////
if (typeof ol_fgcolor=='undefined') var ol_fgcolor="#CCCCFF";
if (typeof ol_bgcolor=='undefined') var ol_bgcolor="#333399";
if (typeof ol_textcolor=='undefined') var ol_textcolor="#000000";
if (typeof ol_capcolor=='undefined') var ol_capcolor="#FFFFFF";
if (typeof ol_closecolor=='undefined') var ol_closecolor="#9999FF";
if (typeof ol_textfont=='undefined') var ol_textfont="Verdana,Arial,Helvetica";
if (typeof ol_captionfont=='undefined') var ol_captionfont="Verdana,Arial,Helvetica";
if (typeof ol_closefont=='undefined') var ol_closefont="Verdana,Arial,Helvetica";
if (typeof ol_textsize=='undefined') var ol_textsize="1";
if (typeof ol_captionsize=='undefined') var ol_captionsize="1";
if (typeof ol_closesize=='undefined') var ol_closesize="1";
if (typeof ol_width=='undefined') var ol_width="200";
if (typeof ol_border=='undefined') var ol_border="1";
if (typeof ol_cellpad=='undefined') var ol_cellpad=2;
if (typeof ol_offsetx=='undefined') var ol_offsetx=10;
if (typeof ol_offsety=='undefined') var ol_offsety=10;
if (typeof ol_text=='undefined') var ol_text="Default Text";
if (typeof ol_cap=='undefined') var ol_cap="";
if (typeof ol_sticky=='undefined') var ol_sticky=0;
if (typeof ol_background=='undefined') var ol_background="";
if (typeof ol_close=='undefined') var ol_close="Close";
if (typeof ol_hpos=='undefined') var ol_hpos=RIGHT;
if (typeof ol_status=='undefined') var ol_status="";
if (typeof ol_autostatus=='undefined') var ol_autostatus=0;
if (typeof ol_height=='undefined') var ol_height=-1;
if (typeof ol_snapx=='undefined') var ol_snapx=0;
if (typeof ol_snapy=='undefined') var ol_snapy=0;
if (typeof ol_fixx=='undefined') var ol_fixx=-1;
if (typeof ol_fixy=='undefined') var ol_fixy=-1;
if (typeof ol_relx=='undefined') var ol_relx=null;
if (typeof ol_rely=='undefined') var ol_rely=null;
if (typeof ol_fgbackground=='undefined') var ol_fgbackground="";
if (typeof ol_bgbackground=='undefined') var ol_bgbackground="";
if (typeof ol_padxl=='undefined') var ol_padxl=1;
if (typeof ol_padxr=='undefined') var ol_padxr=1;
if (typeof ol_padyt=='undefined') var ol_padyt=1;
if (typeof ol_padyb=='undefined') var ol_padyb=1;
if (typeof ol_fullhtml=='undefined') var ol_fullhtml=0;
if (typeof ol_vpos=='undefined') var ol_vpos=BELOW;
if (typeof ol_aboveheight=='undefined') var ol_aboveheight=0;
if (typeof ol_capicon=='undefined') var ol_capicon="";
if (typeof ol_frame=='undefined') var ol_frame=self;
if (typeof ol_timeout=='undefined') var ol_timeout=0;
if (typeof ol_function=='undefined') var ol_function=null;
if (typeof ol_delay=='undefined') var ol_delay=0;
if (typeof ol_hauto=='undefined') var ol_hauto=0;
if (typeof ol_vauto=='undefined') var ol_vauto=0;
if (typeof ol_closeclick=='undefined') var ol_closeclick=0;
if (typeof ol_wrap=='undefined') var ol_wrap=0;
if (typeof ol_followmouse=='undefined') var ol_followmouse=1;
if (typeof ol_mouseoff=='undefined') var ol_mouseoff=0;
if (typeof ol_closetitle=='undefined') var ol_closetitle='Close';
if (typeof ol_compatmode=='undefined') var ol_compatmode=0;
if (typeof ol_css=='undefined') var ol_css=CSSOFF;
if (typeof ol_fgclass=='undefined') var ol_fgclass="";
if (typeof ol_bgclass=='undefined') var ol_bgclass="";
if (typeof ol_textfontclass=='undefined') var ol_textfontclass="";
if (typeof ol_captionfontclass=='undefined') var ol_captionfontclass="";
if (typeof ol_closefontclass=='undefined') var ol_closefontclass="";

////////
// ARRAY CONFIGURATION
////////

// You can use these arrays to store popup text here instead of in the html.
if (typeof ol_texts=='undefined') var ol_texts = new Array("Text 0", "Text 1");
if (typeof ol_caps=='undefined') var ol_caps = new Array("Caption 0", "Caption 1");

////////
// END OF CONFIGURATION
// Don't change anything below this line, all configuration is above.
////////





////////
// INIT
////////
// Runtime variables init. Don't change for config!
var o3_text="";
var o3_cap="";
var o3_sticky=0;
var o3_background="";
var o3_close="Close";
var o3_hpos=RIGHT;
var o3_offsetx=2;
var o3_offsety=2;
var o3_fgcolor="";
var o3_bgcolor="";
var o3_textcolor="";
var o3_capcolor="";
var o3_closecolor="";
var o3_width=100;
var o3_border=1;
var o3_cellpad=2;
var o3_status="";
var o3_autostatus=0;
var o3_height=-1;
var o3_snapx=0;
var o3_snapy=0;
var o3_fixx=-1;
var o3_fixy=-1;
var o3_relx=null;
var o3_rely=null;
var o3_fgbackground="";
var o3_bgbackground="";
var o3_padxl=0;
var o3_padxr=0;
var o3_padyt=0;
var o3_padyb=0;
var o3_fullhtml=0;
var o3_vpos=BELOW;
var o3_aboveheight=0;
var o3_capicon="";
var o3_textfont="Verdana,Arial,Helvetica";
var o3_captionfont="Verdana,Arial,Helvetica";
var o3_closefont="Verdana,Arial,Helvetica";
var o3_textsize="1";
var o3_captionsize="1";
var o3_closesize="1";
var o3_frame=self;
var o3_timeout=0;
var o3_timerid=0;
var o3_allowmove=0;
var o3_function=null; 
var o3_delay=0;
var o3_delayid=0;
var o3_hauto=0;
var o3_vauto=0;
var o3_closeclick=0;
var o3_wrap=0;
var o3_followmouse=1;
var o3_mouseoff=0;
var o3_closetitle='';
var o3_compatmode=0;
var o3_css=CSSOFF;
var o3_fgclass="";
var o3_bgclass="";
var o3_textfontclass="";
var o3_captionfontclass="";
var o3_closefontclass="";

// Display state variables
var o3_x = 0;
var o3_y = 0;
var o3_showingsticky = 0;
var o3_removecounter = 0;

// Our layer
var over = null;
var fnRef, hoveringSwitch = false;
var olHideDelay;

// Decide browser version
var isMac = (navigator.userAgent.indexOf("Mac") != -1);
var olOp = (navigator.userAgent.toLowerCase().indexOf('opera') > -1 && document.createTextNode);  // Opera 7
var olNs4 = (navigator.appName=='Netscape' && parseInt(navigator.appVersion) == 4);
var olNs6 = (document.getElementById) ? true : false;
var olKq = (olNs6 && /konqueror/i.test(navigator.userAgent));
var olIe4 = (document.all) ? true : false;
var olIe5 = false; 
var olIe55 = false; // Added additional variable to identify IE5.5+
var docRoot = 'document.body';

// Resize fix for NS4.x to keep track of layer
if (olNs4) {
	var oW = window.innerWidth;
	var oH = window.innerHeight;
	window.onresize = function() { if (oW != window.innerWidth || oH != window.innerHeight) location.reload(); }
}

// Microsoft Stupidity Check(tm).
if (olIe4) {
	var agent = navigator.userAgent;
	if (/MSIE/.test(agent)) {
		var versNum = parseFloat(agent.match(/MSIE[ ](\d\.\d+)\.*/i)[1]);
		if (versNum >= 5){
			olIe5=true;
			olIe55=(versNum>=5.5&&!olOp) ? true : false;
			if (olNs6) olNs6=false;
		}
	}
	if (olNs6) olIe4 = false;
}

// Check for compatability mode.
if (document.compatMode && document.compatMode == 'CSS1Compat') {
	docRoot= ((olIe4 && !olOp) ? 'document.documentElement' : docRoot);
}

// Add window onload handlers to indicate when all modules have been loaded
// For Netscape 6+ and Mozilla, uses addEventListener method on the window object
// For IE it uses the attachEvent method of the window object and for Netscape 4.x
// it sets the window.onload handler to the OLonload_handler function for Bubbling
if(window.addEventListener) window.addEventListener("load",OLonLoad_handler,false);
else if (window.attachEvent) window.attachEvent("onload",OLonLoad_handler);

var capExtent;

////////
// PUBLIC FUNCTIONS
////////

// overlib(arg0,...,argN)
// Loads parameters into global runtime variables.
function overlib() {
	if (!olLoaded || isExclusive(overlib.arguments)) return true;
	if (olCheckMouseCapture) olMouseCapture();
	if (over) {
		over = (typeof over.id != 'string') ? o3_frame.document.all['overDiv'] : over;
		cClick();
	}

	// Load defaults to runtime.
  olHideDelay=0;
	o3_text=ol_text;
	o3_cap=ol_cap;
	o3_sticky=ol_sticky;
	o3_background=ol_background;
	o3_close=ol_close;
	o3_hpos=ol_hpos;
	o3_offsetx=ol_offsetx;
	o3_offsety=ol_offsety;
	o3_fgcolor=ol_fgcolor;
	o3_bgcolor=ol_bgcolor;
	o3_textcolor=ol_textcolor;
	o3_capcolor=ol_capcolor;
	o3_closecolor=ol_closecolor;
	o3_width=ol_width;
	o3_border=ol_border;
	o3_cellpad=ol_cellpad;
	o3_status=ol_status;
	o3_autostatus=ol_autostatus;
	o3_height=ol_height;
	o3_snapx=ol_snapx;
	o3_snapy=ol_snapy;
	o3_fixx=ol_fixx;
	o3_fixy=ol_fixy;
	o3_relx=ol_relx;
	o3_rely=ol_rely;
	o3_fgbackground=ol_fgbackground;
	o3_bgbackground=ol_bgbackground;
	o3_padxl=ol_padxl;
	o3_padxr=ol_padxr;
	o3_padyt=ol_padyt;
	o3_padyb=ol_padyb;
	o3_fullhtml=ol_fullhtml;
	o3_vpos=ol_vpos;
	o3_aboveheight=ol_aboveheight;
	o3_capicon=ol_capicon;
	o3_textfont=ol_textfont;
	o3_captionfont=ol_captionfont;
	o3_closefont=ol_closefont;
	o3_textsize=ol_textsize;
	o3_captionsize=ol_captionsize;
	o3_closesize=ol_closesize;
	o3_timeout=ol_timeout;
	o3_function=ol_function;
	o3_delay=ol_delay;
	o3_hauto=ol_hauto;
	o3_vauto=ol_vauto;
	o3_closeclick=ol_closeclick;
	o3_wrap=ol_wrap;	
	o3_followmouse=ol_followmouse;
	o3_mouseoff=ol_mouseoff;
	o3_closetitle=ol_closetitle;
	o3_css=ol_css;
	o3_compatmode=ol_compatmode;
	o3_fgclass=ol_fgclass;
	o3_bgclass=ol_bgclass;
	o3_textfontclass=ol_textfontclass;
	o3_captionfontclass=ol_captionfontclass;
	o3_closefontclass=ol_closefontclass;
	
	setRunTimeVariables();
	
	fnRef = '';
	
	// Special for frame support, over must be reset...
	o3_frame = ol_frame;
	
	if(!(over=createDivContainer())) return false;

	parseTokens('o3_', overlib.arguments);
	if (!postParseChecks()) return false;

	if (o3_delay == 0) {
		return runHook("olMain", FREPLACE);
 	} else {
		o3_delayid = setTimeout("runHook('olMain', FREPLACE)", o3_delay);
		return false;
	}
}

// Clears popups if appropriate
function nd(time) {
	if (olLoaded && !isExclusive()) {
		hideDelay(time);  // delay popup close if time specified

		if (o3_removecounter >= 1) { o3_showingsticky = 0 };
		
		if (o3_showingsticky == 0) {
			o3_allowmove = 0;
			if (over != null && o3_timerid == 0) runHook("hideObject", FREPLACE, over);
		} else {
			o3_removecounter++;
		}
	}
	
	return true;
}

// The Close onMouseOver function for stickies
function cClick() {
	if (olLoaded) {
		runHook("hideObject", FREPLACE, over);
		o3_showingsticky = 0;	
	}	
	return false;
}

// Method for setting page specific defaults.
function overlib_pagedefaults() {
	parseTokens('ol_', overlib_pagedefaults.arguments);
}


////////
// OVERLIB MAIN FUNCTION
////////

// This function decides what it is we want to display and how we want it done.
function olMain() {
	var layerhtml, styleType;
 	runHook("olMain", FBEFORE);
 	
	if (o3_background!="" || o3_fullhtml) {
		// Use background instead of box.
		layerhtml = runHook('ol_content_background', FALTERNATE, o3_css, o3_text, o3_background, o3_fullhtml);
	} else {
		// They want a popup box.
		styleType = (pms[o3_css-1-pmStart] == "cssoff" || pms[o3_css-1-pmStart] == "cssclass");

		// Prepare popup background
		if (o3_fgbackground != "") o3_fgbackground = "background=\""+o3_fgbackground+"\"";
		if (o3_bgbackground != "") o3_bgbackground = (styleType ? "background=\""+o3_bgbackground+"\"" : o3_bgbackground);

		// Prepare popup colors
		if (o3_fgcolor != "") o3_fgcolor = (styleType ? "bgcolor=\""+o3_fgcolor+"\"" : o3_fgcolor);
		if (o3_bgcolor != "") o3_bgcolor = (styleType ? "bgcolor=\""+o3_bgcolor+"\"" : o3_bgcolor);

		// Prepare popup height
		if (o3_height > 0) o3_height = (styleType ? "height=\""+o3_height+"\"" : o3_height);
		else o3_height = "";

		// Decide which kinda box.
		if (o3_cap=="") {
			// Plain
			layerhtml = runHook('ol_content_simple', FALTERNATE, o3_css, o3_text);
		} else {
			// With caption
			if (o3_sticky) {
				// Show close text
				layerhtml = runHook('ol_content_caption', FALTERNATE, o3_css, o3_text, o3_cap, o3_close);
			} else {
				// No close text
				layerhtml = runHook('ol_content_caption', FALTERNATE, o3_css, o3_text, o3_cap, "");
			}
		}
	}	

	// We want it to stick!
	if (o3_sticky) {
		if (o3_timerid > 0) {
			clearTimeout(o3_timerid);
			o3_timerid = 0;
		}
		o3_showingsticky = 1;
		o3_removecounter = 0;
	}

	// Created a separate routine to generate the popup to make it easier
	// to implement a plugin capability
	if (!runHook("createPopup", FREPLACE, layerhtml)) return false;

	// Prepare status bar
	if (o3_autostatus > 0) {
		o3_status = o3_text;
		if (o3_autostatus > 1) o3_status = o3_cap;
	}

	// When placing the layer the first time, even stickies may be moved.
	o3_allowmove = 0;

	// Initiate a timer for timeout
	if (o3_timeout > 0) {          
		if (o3_timerid > 0) clearTimeout(o3_timerid);
		o3_timerid = setTimeout("cClick()", o3_timeout);
	}

	// Show layer
	runHook("disp", FREPLACE, o3_status);
	runHook("olMain", FAFTER);

	return (olOp && event && event.type == 'mouseover' && !o3_status) ? '' : (o3_status != '');
}

////////
// LAYER GENERATION FUNCTIONS
////////
// These functions just handle popup content with tags that should adhere to the W3C standards specification.

// Makes simple table without caption
function ol_content_simple(text) {
	var cpIsMultiple = /,/.test(o3_cellpad);
	var txt = '<table width="'+o3_width+ '" border="0" cellpadding="'+o3_border+'" cellspacing="0" '+(o3_bgclass ? 'class="'+o3_bgclass+'"' : o3_bgcolor+' '+o3_height)+'><tr><td><table width="100%" border="0" '+((olNs4||!cpIsMultiple) ? 'cellpadding="'+o3_cellpad+'" ' : '')+'cellspacing="0" '+(o3_fgclass ? 'class="'+o3_fgclass+'"' : o3_fgcolor+' '+o3_fgbackground+' '+o3_height)+'><tr><td valign="TOP"'+(o3_textfontclass ? ' class="'+o3_textfontclass+'">' : ((!olNs4&&cpIsMultiple) ? ' style="'+setCellPadStr(o3_cellpad)+'">' : '>'))+(o3_textfontclass ? '' : wrapStr(0,o3_textsize,'text'))+text+(o3_textfontclass ? '' : wrapStr(1,o3_textsize))+'</td></tr></table></td></tr></table>';

	set_background("");
	return txt;
}

// Makes table with caption and optional close link
function ol_content_caption(text,title,close) {
	var nameId, txt, cpIsMultiple = /,/.test(o3_cellpad);
	var closing, closeevent;

	closing = "";
	closeevent = "onmouseover";
	if (o3_closeclick == 1) closeevent = (o3_closetitle ? "title='" + o3_closetitle +"'" : "") + " onclick";
	if (o3_capicon != "") {
	  nameId = ' hspace = \"5\"'+' align = \"middle\" alt = \"\"';
	  if (typeof o3_dragimg != 'undefined' && o3_dragimg) nameId =' hspace=\"5\"'+' name=\"'+o3_dragimg+'\" id=\"'+o3_dragimg+'\" align=\"middle\" alt=\"Drag Enabled\" title=\"Drag Enabled\"';
	  o3_capicon = '<img src=\"'+o3_capicon+'\"'+nameId+' />';
	}

	if (close != "")
		closing = '<td '+(!o3_compatmode && o3_closefontclass ? 'class="'+o3_closefontclass : 'align="RIGHT')+'"><a href="javascript:return '+fnRef+'cClick();"'+((o3_compatmode && o3_closefontclass) ? ' class="' + o3_closefontclass + '" ' : ' ')+closeevent+'="return '+fnRef+'cClick();">'+(o3_closefontclass ? '' : wrapStr(0,o3_closesize,'close'))+close+(o3_closefontclass ? '' : wrapStr(1,o3_closesize,'close'))+'</a></td>';
	txt = '<table width="'+o3_width+ '" border="0" cellpadding="'+o3_border+'" cellspacing="0" '+(o3_bgclass ? 'class="'+o3_bgclass+'"' : o3_bgcolor+' '+o3_bgbackground+' '+o3_height)+'><tr><td><table width="100%" border="0" cellpadding="2" cellspacing="0"><tr><td'+(o3_captionfontclass ? ' class="'+o3_captionfontclass+'">' : '>')+(o3_captionfontclass ? '' : '<b>'+wrapStr(0,o3_captionsize,'caption'))+o3_capicon+title+(o3_captionfontclass ? '' : wrapStr(1,o3_captionsize)+'</b>')+'</td>'+closing+'</tr></table><table width="100%" border="0" '+((olNs4||!cpIsMultiple) ? 'cellpadding="'+o3_cellpad+'" ' : '')+'cellspacing="0" '+(o3_fgclass ? 'class="'+o3_fgclass+'"' : o3_fgcolor+' '+o3_fgbackground+' '+o3_height)+'><tr><td valign="TOP"'+(o3_textfontclass ? ' class="'+o3_textfontclass+'">' :((!olNs4&&cpIsMultiple) ? ' style="'+setCellPadStr(o3_cellpad)+'">' : '>'))+(o3_textfontclass ? '' : wrapStr(0,o3_textsize,'text'))+text+(o3_textfontclass ? '' : wrapStr(1,o3_textsize)) + '</td></tr></table></td></tr></table>';

	set_background("");
	return txt;
}

// Sets the background picture,padding and lots more. :)
function ol_content_background(text,picture,hasfullhtml) {
	if (hasfullhtml) {
		txt=text;
	} else {
		txt='<table width="'+o3_width+'" border="0" cellpadding="0" cellspacing="0" height="'+o3_height+'"><tr><td colspan="3" height="'+o3_padyt+'"></td></tr><tr><td width="'+o3_padxl+'"></td><td valign="TOP" width="'+(o3_width-o3_padxl-o3_padxr)+(o3_textfontclass ? '" class="'+o3_textfontclass : '')+'">'+(o3_textfontclass ? '' : wrapStr(0,o3_textsize,'text'))+text+(o3_textfontclass ? '' : wrapStr(1,o3_textsize))+'</td><td width="'+o3_padxr+'"></td></tr><tr><td colspan="3" height="'+o3_padyb+'"></td></tr></table>';
	}

	set_background(picture);
	return txt;
}

// Loads a picture into the div.
function set_background(pic) {
	if (pic == "") {
		if (olNs4) {
			over.background.src = null; 
		} else if (over.style) {
			over.style.backgroundImage = "none";
		}
	} else {
		if (olNs4) {
			over.background.src = pic;
		} else if (over.style) {
			over.style.width=o3_width + 'px';
			over.style.backgroundImage = "url("+pic+")";
		}
	}
}

////////
// HANDLING FUNCTIONS
////////
var olShowId=-1;

// Displays the popup
function disp(statustext) {
	runHook("disp", FBEFORE);
	
	if (o3_allowmove == 0) {
		runHook("placeLayer", FREPLACE);
		(olNs6&&olShowId<0) ? olShowId=setTimeout("runHook('showObject', FREPLACE, over)", 1) : runHook("showObject", FREPLACE, over);
		o3_allowmove = (o3_sticky || o3_followmouse==0) ? 0 : 1;
	}
	
	runHook("disp", FAFTER);

	if (statustext != "") self.status = statustext;
}

// Creates the actual popup structure
function createPopup(lyrContent){
	runHook("createPopup", FBEFORE);
	
	if (o3_wrap) {
		var wd,ww,theObj = (olNs4 ? over : over.style);
		theObj.top = theObj.left = ((olIe4&&!olOp) ? 0 : -10000) + (!olNs4 ? 'px' : 0);
		layerWrite(lyrContent);
		wd = (olNs4 ? over.clip.width : over.offsetWidth);
		if (wd > (ww=windowWidth())) {
			lyrContent=lyrContent.replace(/\&nbsp;/g, ' ');
			o3_width=ww;
			o3_wrap=0;
		} 
	}

	layerWrite(lyrContent);
	
	// Have to set o3_width for placeLayer() routine if o3_wrap is turned on
	if (o3_wrap) o3_width=(olNs4 ? over.clip.width : over.offsetWidth);
	
	runHook("createPopup", FAFTER, lyrContent);

	return true;
}

// Decides where we want the popup.
function placeLayer() {
	var placeX, placeY, widthFix = 0;
	
	// HORIZONTAL PLACEMENT, re-arranged to work in Safari
	if (o3_frame.innerWidth) widthFix=18; 
	iwidth = windowWidth();

	// Horizontal scroll offset
	winoffset=(olIe4) ? eval('o3_frame.'+docRoot+'.scrollLeft') : o3_frame.pageXOffset;

	placeX = runHook('horizontalPlacement',FCHAIN,iwidth,winoffset,widthFix);

	// VERTICAL PLACEMENT, re-arranged to work in Safari
	if (o3_frame.innerHeight) {
		iheight=o3_frame.innerHeight;
	} else if (eval('o3_frame.'+docRoot)&&eval("typeof o3_frame."+docRoot+".clientHeight=='number'")&&eval('o3_frame.'+docRoot+'.clientHeight')) { 
		iheight=eval('o3_frame.'+docRoot+'.clientHeight');
	}			

	// Vertical scroll offset
	scrolloffset=(olIe4) ? eval('o3_frame.'+docRoot+'.scrollTop') : o3_frame.pageYOffset;
	placeY = runHook('verticalPlacement',FCHAIN,iheight,scrolloffset);

	// Actually move the object.
	repositionTo(over, placeX, placeY);
}

// Moves the layer
function olMouseMove(e) {
	var e = (e) ? e : event;

	if (e.pageX) {
		o3_x = e.pageX;
		o3_y = e.pageY;
	} else if (e.clientX) {
		o3_x = eval('e.clientX+o3_frame.'+docRoot+'.scrollLeft');
		o3_y = eval('e.clientY+o3_frame.'+docRoot+'.scrollTop');
	}
	
	if (o3_allowmove == 1) runHook("placeLayer", FREPLACE);

	// MouseOut handler
	if (hoveringSwitch && !olNs4 && runHook("cursorOff", FREPLACE)) {
		(olHideDelay ? hideDelay(olHideDelay) : cClick());
		hoveringSwitch = !hoveringSwitch;
	}
}

// Fake function for 3.0 users.
function no_overlib() { return ver3fix; }

// Capture the mouse and chain other scripts.
function olMouseCapture() {
	capExtent = document;
	var fN, str = '', l, k, f, wMv, sS, mseHandler = olMouseMove;
	var re = /function[ ]*(\w*)\(/;
	
	wMv = (!olIe4 && window.onmousemove);
	if (document.onmousemove || wMv) {
		if (wMv) capExtent = window;
		f = capExtent.onmousemove.toString();
		fN = f.match(re);
		if (fN == null) {
			str = f+'(e); ';
		} else if (fN[1] == 'anonymous' || fN[1] == 'olMouseMove' || (wMv && fN[1] == 'onmousemove')) {
			if (!olOp && wMv) {
				l = f.indexOf('{')+1;
				k = f.lastIndexOf('}');
				sS = f.substring(l,k);
				if ((l = sS.indexOf('(')) != -1) {
					sS = sS.substring(0,l).replace(/^\s+/,'').replace(/\s+$/,'');
					if (eval("typeof " + sS + " == 'undefined'")) window.onmousemove = null;
					else str = sS + '(e);';
				}
			}
			if (!str) {
				olCheckMouseCapture = false;
				return;
			}
		} else {
			if (fN[1]) str = fN[1]+'(e); ';
			else {
				l = f.indexOf('{')+1;
				k = f.lastIndexOf('}');
				str = f.substring(l,k) + '\n';
			}
		}
		str += 'olMouseMove(e); ';
		mseHandler = new Function('e', str);
	}

	capExtent.onmousemove = mseHandler;
	if (olNs4) capExtent.captureEvents(Event.MOUSEMOVE);
}

////////
// PARSING FUNCTIONS
////////

// Does the actual command parsing.
function parseTokens(pf, ar) {
	// What the next argument is expected to be.
	var v, i, mode=-1, par = (pf != 'ol_');	
	var fnMark = (par && !ar.length ? 1 : 0);

	for (i = 0; i < ar.length; i++) {
		if (mode < 0) {
			// Arg is maintext,unless its a number between pmStart and pmUpper
			// then its a command.
			if (typeof ar[i] == 'number' && ar[i] > pmStart && ar[i] < pmUpper) {
				fnMark = (par ? 1 : 0);
				i--;   // backup one so that the next block can parse it
			} else {
				switch(pf) {
					case 'ol_':
						ol_text = ar[i].toString();
						break;
					default:
						o3_text=ar[i].toString();  
				}
			}
			mode = 0;
		} else {
			// Note: NS4 doesn't like switch cases with vars.
			if (ar[i] >= pmCount || ar[i]==DONOTHING) { continue; }
			if (ar[i]==INARRAY) { fnMark = 0; eval(pf+'text=ol_texts['+ar[++i]+'].toString()'); continue; }
			if (ar[i]==CAPARRAY) { eval(pf+'cap=ol_caps['+ar[++i]+'].toString()'); continue; }
			if (ar[i]==STICKY) { if (pf!='ol_') eval(pf+'sticky=1'); continue; }
			if (ar[i]==BACKGROUND) { eval(pf+'background="'+ar[++i]+'"'); continue; }
			if (ar[i]==NOCLOSE) { if (pf!='ol_') opt_NOCLOSE(); continue; }
			if (ar[i]==CAPTION) { eval(pf+"cap='"+escSglQuote(ar[++i])+"'"); continue; }
			if (ar[i]==CENTER || ar[i]==LEFT || ar[i]==RIGHT) { eval(pf+'hpos='+ar[i]); if(pf!='ol_') olHautoFlag=1; continue; }
			if (ar[i]==OFFSETX) { eval(pf+'offsetx='+ar[++i]); continue; }
			if (ar[i]==OFFSETY) { eval(pf+'offsety='+ar[++i]); continue; }
			if (ar[i]==FGCOLOR) { eval(pf+'fgcolor="'+ar[++i]+'"'); continue; }
			if (ar[i]==BGCOLOR) { eval(pf+'bgcolor="'+ar[++i]+'"'); continue; }
			if (ar[i]==TEXTCOLOR) { eval(pf+'textcolor="'+ar[++i]+'"'); continue; }
			if (ar[i]==CAPCOLOR) { eval(pf+'capcolor="'+ar[++i]+'"'); continue; }
			if (ar[i]==CLOSECOLOR) { eval(pf+'closecolor="'+ar[++i]+'"'); continue; }
			if (ar[i]==WIDTH) { eval(pf+'width='+ar[++i]); continue; }
			if (ar[i]==BORDER) { eval(pf+'border='+ar[++i]); continue; }
			if (ar[i]==CELLPAD) { i=opt_MULTIPLEARGS(++i,ar,(pf+'cellpad')); continue; }
			if (ar[i]==STATUS) { eval(pf+"status='"+escSglQuote(ar[++i])+"'"); continue; }
			if (ar[i]==AUTOSTATUS) { eval(pf +'autostatus=('+pf+'autostatus == 1) ? 0 : 1'); continue; }
			if (ar[i]==AUTOSTATUSCAP) { eval(pf +'autostatus=('+pf+'autostatus == 2) ? 0 : 2'); continue; }
			if (ar[i]==HEIGHT) { eval(pf+'height='+pf+'aboveheight='+ar[++i]); continue; } // Same param again.
			if (ar[i]==CLOSETEXT) { eval(pf+"close='"+escSglQuote(ar[++i])+"'"); continue; }
			if (ar[i]==SNAPX) { eval(pf+'snapx='+ar[++i]); continue; }
			if (ar[i]==SNAPY) { eval(pf+'snapy='+ar[++i]); continue; }
			if (ar[i]==FIXX) { eval(pf+'fixx='+ar[++i]); continue; }
			if (ar[i]==FIXY) { eval(pf+'fixy='+ar[++i]); continue; }
			if (ar[i]==RELX) { eval(pf+'relx='+ar[++i]); continue; }
			if (ar[i]==RELY) { eval(pf+'rely='+ar[++i]); continue; }
			if (ar[i]==FGBACKGROUND) { eval(pf+'fgbackground="'+ar[++i]+'"'); continue; }
			if (ar[i]==BGBACKGROUND) { eval(pf+'bgbackground="'+ar[++i]+'"'); continue; }
			if (ar[i]==PADX) { eval(pf+'padxl='+ar[++i]); eval(pf+'padxr='+ar[++i]); continue; }
			if (ar[i]==PADY) { eval(pf+'padyt='+ar[++i]); eval(pf+'padyb='+ar[++i]); continue; }
			if (ar[i]==FULLHTML) { if (pf!='ol_') eval(pf+'fullhtml=1'); continue; }
			if (ar[i]==BELOW || ar[i]==ABOVE) { eval(pf+'vpos='+ar[i]); if (pf!='ol_') olVautoFlag=1; continue; }
			if (ar[i]==CAPICON) { eval(pf+'capicon="'+ar[++i]+'"'); continue; }
			if (ar[i]==TEXTFONT) { eval(pf+"textfont='"+escSglQuote(ar[++i])+"'"); continue; }
			if (ar[i]==CAPTIONFONT) { eval(pf+"captionfont='"+escSglQuote(ar[++i])+"'"); continue; }
			if (ar[i]==CLOSEFONT) { eval(pf+"closefont='"+escSglQuote(ar[++i])+"'"); continue; }
			if (ar[i]==TEXTSIZE) { eval(pf+'textsize="'+ar[++i]+'"'); continue; }
			if (ar[i]==CAPTIONSIZE) { eval(pf+'captionsize="'+ar[++i]+'"'); continue; }
			if (ar[i]==CLOSESIZE) { eval(pf+'closesize="'+ar[++i]+'"'); continue; }
			if (ar[i]==TIMEOUT) { eval(pf+'timeout='+ar[++i]); continue; }
			if (ar[i]==FUNCTION) { if (pf=='ol_') { if (typeof ar[i+1]!='number') { v=ar[++i]; ol_function=(typeof v=='function' ? v : null); }} else {fnMark = 0; v = null; if (typeof ar[i+1]!='number') v = ar[++i];  opt_FUNCTION(v); } continue; }
			if (ar[i]==DELAY) { eval(pf+'delay='+ar[++i]); continue; }
			if (ar[i]==HAUTO) { eval(pf+'hauto=('+pf+'hauto == 0) ? 1 : 0'); continue; }
			if (ar[i]==VAUTO) { eval(pf+'vauto=('+pf+'vauto == 0) ? 1 : 0'); continue; }
			if (ar[i]==CLOSECLICK) { eval(pf +'closeclick=('+pf+'closeclick == 0) ? 1 : 0'); continue; }
			if (ar[i]==WRAP) { eval(pf +'wrap=('+pf+'wrap == 0) ? 1 : 0'); continue; }
			if (ar[i]==FOLLOWMOUSE) { eval(pf +'followmouse=('+pf+'followmouse == 1) ? 0 : 1'); continue; }
			if (ar[i]==MOUSEOFF) { eval(pf +'mouseoff=('+pf+'mouseoff==0) ? 1 : 0'); v=ar[i+1]; if (pf != 'ol_' && eval(pf+'mouseoff') && typeof v == 'number' && (v < pmStart || v > pmUpper)) olHideDelay=ar[++i]; continue; }
			if (ar[i]==CLOSETITLE) { eval(pf+"closetitle='"+escSglQuote(ar[++i])+"'"); continue; }
			if (ar[i]==CSSOFF||ar[i]==CSSCLASS) { eval(pf+'css='+ar[i]); continue; }
			if (ar[i]==COMPATMODE) { eval(pf+'compatmode=('+pf+'compatmode==0) ? 1 : 0'); continue; }
			if (ar[i]==FGCLASS) { eval(pf+'fgclass="'+ar[++i]+'"'); continue; }
			if (ar[i]==BGCLASS) { eval(pf+'bgclass="'+ar[++i]+'"'); continue; }
			if (ar[i]==TEXTFONTCLASS) { eval(pf+'textfontclass="'+ar[++i]+'"'); continue; }
			if (ar[i]==CAPTIONFONTCLASS) { eval(pf+'captionfontclass="'+ar[++i]+'"'); continue; }
			if (ar[i]==CLOSEFONTCLASS) { eval(pf+'closefontclass="'+ar[++i]+'"'); continue; }
			i = parseCmdLine(pf, i, ar);
		}
	}

	if (fnMark && o3_function) o3_text = o3_function();
	
	if ((pf == 'o3_') && o3_wrap) {
		o3_width = 0;
		
		var tReg=/<.*\n*>/ig;
		if (!tReg.test(o3_text)) o3_text = o3_text.replace(/[ ]+/g, '&nbsp;');
		if (!tReg.test(o3_cap))o3_cap = o3_cap.replace(/[ ]+/g, '&nbsp;');
	}
	if ((pf == 'o3_') && o3_sticky) {
		if (!o3_close && (o3_frame != ol_frame)) o3_close = ol_close;
		if (o3_mouseoff && (o3_frame == ol_frame)) opt_NOCLOSE(' ');
	}
}


////////
// LAYER FUNCTIONS
////////

// Writes to a layer
function layerWrite(txt) {
	txt += "\n";
	if (olNs4) {
		var lyr = o3_frame.document.layers['overDiv'].document
		lyr.write(txt)
		lyr.close()
	} else if (typeof over.innerHTML != 'undefined') {
		if (olIe5 && isMac) over.innerHTML = '';
		over.innerHTML = txt;
	} else {
		range = o3_frame.document.createRange();
		range.setStartAfter(over);
		domfrag = range.createContextualFragment(txt);
		
		while (over.hasChildNodes()) {
			over.removeChild(over.lastChild);
		}
		
		over.appendChild(domfrag);
	}
}

// Make an object visible
function showObject(obj) {
	runHook("showObject", FBEFORE);

	var theObj=(olNs4 ? obj : obj.style);
	theObj.visibility = 'visible';

	runHook("showObject", FAFTER);
}

// Hides an object
function hideObject(obj) {
	runHook("hideObject", FBEFORE);

	var theObj=(olNs4 ? obj : obj.style);
	if (olNs6 && olShowId>0) { clearTimeout(olShowId); olShowId=0; }
	theObj.visibility = 'hidden';
	theObj.top = theObj.left = ((olIe4&&!olOp) ? 0 : -10000) + (!olNs4 ? 'px' : 0);

	if (o3_timerid > 0) clearTimeout(o3_timerid);
	if (o3_delayid > 0) clearTimeout(o3_delayid);

	o3_timerid = 0;
	o3_delayid = 0;
	self.status = "";

	if (obj.onmouseout||obj.onmouseover) {
		if (olNs4) obj.releaseEvents(Event.MOUSEOUT || Event.MOUSEOVER);
		obj.onmouseout = obj.onmouseover = null;
	}

	runHook("hideObject", FAFTER);
}

// Move a layer
function repositionTo(obj, xL, yL) {
	var theObj=(olNs4 ? obj : obj.style);
	theObj.left = xL + (!olNs4 ? 'px' : 0);
	theObj.top = yL + (!olNs4 ? 'px' : 0);
}

// Check position of cursor relative to overDiv DIVision; mouseOut function
function cursorOff() {
	var left = parseInt(over.style.left);
	var top = parseInt(over.style.top);
	var right = left + (over.offsetWidth >= parseInt(o3_width) ? over.offsetWidth : parseInt(o3_width));
	var bottom = top + (over.offsetHeight >= o3_aboveheight ? over.offsetHeight : o3_aboveheight);

	if (o3_x < left || o3_x > right || o3_y < top || o3_y > bottom) return true;

	return false;
}


////////
// COMMAND FUNCTIONS
////////

// Calls callme or the default function.
function opt_FUNCTION(callme) {
	o3_text = (callme ? (typeof callme=='string' ? (/.+\(.*\)/.test(callme) ? eval(callme) : callme) : callme()) : (o3_function ? o3_function() : 'No Function'));

	return 0;
}

// Handle hovering
function opt_NOCLOSE(unused) {
	if (!unused) o3_close = "";

	if (olNs4) {
		over.captureEvents(Event.MOUSEOUT || Event.MOUSEOVER);
		over.onmouseover = function () { if (o3_timerid > 0) { clearTimeout(o3_timerid); o3_timerid = 0; } }
		over.onmouseout = function (e) { if (olHideDelay) hideDelay(olHideDelay); else cClick(e); }
	} else {
		over.onmouseover = function () {hoveringSwitch = true; if (o3_timerid > 0) { clearTimeout(o3_timerid); o3_timerid =0; } }
	}

	return 0;
}

// Function to scan command line arguments for multiples
function opt_MULTIPLEARGS(i, args, parameter) {
  var k=i, re, pV, str='';

  for(k=i; k<args.length; k++) {
		if(typeof args[k] == 'number' && args[k]>pmStart) break;
		str += args[k] + ',';
	}
	if (str) str = str.substring(0,--str.length);

	k--;  // reduce by one so the for loop this is in works correctly
	pV=(olNs4 && /cellpad/i.test(parameter)) ? str.split(',')[0] : str;
	eval(parameter + '="' + pV + '"');

	return k;
}

// Remove &nbsp; in texts when done.
function nbspCleanup() {
	if (o3_wrap) {
		o3_text = o3_text.replace(/\&nbsp;/g, ' ');
		o3_cap = o3_cap.replace(/\&nbsp;/g, ' ');
	}
}

// Escape embedded single quotes in text strings
function escSglQuote(str) {
  return str.toString().replace(/'/g,"\\'");
}

// Onload handler for window onload event
function OLonLoad_handler(e) {
	var re = /\w+\(.*\)[;\s]+/g, olre = /overlib\(|nd\(|cClick\(/, fn, l, i;

	if(!olLoaded) olLoaded=1;

  // Remove it for Gecko based browsers
	if(window.removeEventListener && e.eventPhase == 3) window.removeEventListener("load",OLonLoad_handler,false);
	else if(window.detachEvent) { // and for IE and Opera 4.x but execute calls to overlib, nd, or cClick()
		window.detachEvent("onload",OLonLoad_handler);
		var fN = document.body.getAttribute('onload');
		if (fN) {
			fN=fN.toString().match(re);
			if (fN && fN.length) {
				for (i=0; i<fN.length; i++) {
					if (/anonymous/.test(fN[i])) continue;
					while((l=fN[i].search(/\)[;\s]+/)) != -1) {
						fn=fN[i].substring(0,l+1);
						fN[i] = fN[i].substring(l+2);
						if (olre.test(fn)) eval(fn);
					}
				}
			}
		}
	}
}

// Wraps strings in Layer Generation Functions with the correct tags
//    endWrap true(if end tag) or false if start tag
//    fontSizeStr - font size string such as '1' or '10px'
//    whichString is being wrapped -- 'text', 'caption', or 'close'
function wrapStr(endWrap,fontSizeStr,whichString) {
	var fontStr, fontColor, isClose=((whichString=='close') ? 1 : 0), hasDims=/[%\-a-z]+$/.test(fontSizeStr);
	fontSizeStr = (olNs4) ? (!hasDims ? fontSizeStr : '1') : fontSizeStr;
	if (endWrap) return (hasDims&&!olNs4) ? (isClose ? '</span>' : '</div>') : '</font>';
	else {
		fontStr='o3_'+whichString+'font';
		fontColor='o3_'+((whichString=='caption')? 'cap' : whichString)+'color';
		return (hasDims&&!olNs4) ? (isClose ? '<span style="font-family: '+quoteMultiNameFonts(eval(fontStr))+'; color: '+eval(fontColor)+'; font-size: '+fontSizeStr+';">' : '<div style="font-family: '+quoteMultiNameFonts(eval(fontStr))+'; color: '+eval(fontColor)+'; font-size: '+fontSizeStr+';">') : '<font face="'+eval(fontStr)+'" color="'+eval(fontColor)+'" size="'+(parseInt(fontSizeStr)>7 ? '7' : fontSizeStr)+'">';
	}
}

// Quotes Multi word font names; needed for CSS Standards adherence in font-family
function quoteMultiNameFonts(theFont) {
	var v, pM=theFont.split(',');
	for (var i=0; i<pM.length; i++) {
		v=pM[i];
		v=v.replace(/^\s+/,'').replace(/\s+$/,'');
		if(/\s/.test(v) && !/['"]/.test(v)) {
			v="\'"+v+"\'";
			pM[i]=v;
		}
	}
	return pM.join();
}

// dummy function which will be overridden 
function isExclusive(args) {
	return false;
}

// Sets cellpadding style string value
function setCellPadStr(parameter) {
	var Str='', j=0, ary = new Array(), top, bottom, left, right;

	Str+='padding: ';
	ary=parameter.replace(/\s+/g,'').split(',');

	switch(ary.length) {
		case 2:
			top=bottom=ary[j];
			left=right=ary[++j];
			break;
		case 3:
			top=ary[j];
			left=right=ary[++j];
			bottom=ary[++j];
			break;
		case 4:
			top=ary[j];
			right=ary[++j];
			bottom=ary[++j];
			left=ary[++j];
			break;
	}

	Str+= ((ary.length==1) ? ary[0] + 'px;' : top + 'px ' + right + 'px ' + bottom + 'px ' + left + 'px;');

	return Str;
}

// function will delay close by time milliseconds
function hideDelay(time) {
	if (time&&!o3_delay) {
		if (o3_timerid > 0) clearTimeout(o3_timerid);

		o3_timerid=setTimeout("cClick()",(o3_timeout=time));
	}
}

// Was originally in the placeLayer() routine; separated out for future ease
function horizontalPlacement(browserWidth, horizontalScrollAmount, widthFix) {
	var placeX, iwidth=browserWidth, winoffset=horizontalScrollAmount;
	var parsedWidth = parseInt(o3_width);

	if (o3_fixx > -1 || o3_relx != null) {
		// Fixed position
		placeX=(o3_relx != null ? ( o3_relx < 0 ? winoffset +o3_relx+ iwidth - parsedWidth - widthFix : winoffset+o3_relx) : o3_fixx);
	} else {  
		// If HAUTO, decide what to use.
		if (o3_hauto == 1) {
			if ((o3_x - winoffset) > (iwidth / 2)) {
				o3_hpos = LEFT;
			} else {
				o3_hpos = RIGHT;
			}
		}  		

		// From mouse
		if (o3_hpos == CENTER) { // Center
			placeX = o3_x+o3_offsetx-(parsedWidth/2);

			if (placeX < winoffset) placeX = winoffset;
		}

		if (o3_hpos == RIGHT) { // Right
			placeX = o3_x+o3_offsetx;

			if ((placeX+parsedWidth) > (winoffset+iwidth - widthFix)) {
				placeX = iwidth+winoffset - parsedWidth - widthFix;
				if (placeX < 0) placeX = 0;
			}
		}
		if (o3_hpos == LEFT) { // Left
			placeX = o3_x-o3_offsetx-parsedWidth;
			if (placeX < winoffset) placeX = winoffset;
		}  	

		// Snapping!
		if (o3_snapx > 1) {
			var snapping = placeX % o3_snapx;

			if (o3_hpos == LEFT) {
				placeX = placeX - (o3_snapx+snapping);
			} else {
				// CENTER and RIGHT
				placeX = placeX+(o3_snapx - snapping);
			}

			if (placeX < winoffset) placeX = winoffset;
		}
	}	

	return placeX;
}

// was originally in the placeLayer() routine; separated out for future ease
function verticalPlacement(browserHeight,verticalScrollAmount) {
	var placeY, iheight=browserHeight, scrolloffset=verticalScrollAmount;
	var parsedHeight=(o3_aboveheight ? parseInt(o3_aboveheight) : (olNs4 ? over.clip.height : over.offsetHeight));

	if (o3_fixy > -1 || o3_rely != null) {
		// Fixed position
		placeY=(o3_rely != null ? (o3_rely < 0 ? scrolloffset+o3_rely+iheight - parsedHeight : scrolloffset+o3_rely) : o3_fixy);
	} else {
		// If VAUTO, decide what to use.
		if (o3_vauto == 1) {
			if ((o3_y - scrolloffset) > (iheight / 2) && o3_vpos == BELOW && (o3_y + parsedHeight + o3_offsety - (scrolloffset + iheight) > 0)) {
				o3_vpos = ABOVE;
			} else if (o3_vpos == ABOVE && (o3_y - (parsedHeight + o3_offsety) - scrolloffset < 0)) {
				o3_vpos = BELOW;
			}
		}

		// From mouse
		if (o3_vpos == ABOVE) {
			if (o3_aboveheight == 0) o3_aboveheight = parsedHeight; 

			placeY = o3_y - (o3_aboveheight+o3_offsety);
			if (placeY < scrolloffset) placeY = scrolloffset;
		} else {
			// BELOW
			placeY = o3_y+o3_offsety;
		} 

		// Snapping!
		if (o3_snapy > 1) {
			var snapping = placeY % o3_snapy;  			

			if (o3_aboveheight > 0 && o3_vpos == ABOVE) {
				placeY = placeY - (o3_snapy+snapping);
			} else {
				placeY = placeY+(o3_snapy - snapping);
			} 			

			if (placeY < scrolloffset) placeY = scrolloffset;
		}
	}

	return placeY;
}

// checks positioning flags
function checkPositionFlags() {
	if (olHautoFlag) olHautoFlag = o3_hauto=0;
	if (olVautoFlag) olVautoFlag = o3_vauto=0;
	return true;
}

// get Browser window width
function windowWidth() {
	var w;
	if (o3_frame.innerWidth) w=o3_frame.innerWidth;
	else if (eval('o3_frame.'+docRoot)&&eval("typeof o3_frame."+docRoot+".clientWidth=='number'")&&eval('o3_frame.'+docRoot+'.clientWidth')) 
		w=eval('o3_frame.'+docRoot+'.clientWidth');
	return w;			
}

// create the div container for popup content if it doesn't exist
function createDivContainer(id,frm,zValue) {
	id = (id || 'overDiv'), frm = (frm || o3_frame), zValue = (zValue || 1000);
	var objRef, divContainer = layerReference(id);

	if (divContainer == null) {
		if (olNs4) {
			divContainer = frm.document.layers[id] = new Layer(window.innerWidth, frm);
			objRef = divContainer;
		} else {
			var body = (olIe4 ? frm.document.all.tags('BODY')[0] : frm.document.getElementsByTagName("BODY")[0]);
			if (olIe4&&!document.getElementById) {
				body.insertAdjacentHTML("beforeEnd",'<div id="'+id+'"></div>');
				divContainer=layerReference(id);
			} else {
				divContainer = frm.document.createElement("DIV");
				divContainer.id = id;
				body.appendChild(divContainer);
			}
			objRef = divContainer.style;
		}

		objRef.position = 'absolute';
		objRef.visibility = 'hidden';
		objRef.zIndex = zValue;
		if (olIe4&&!olOp) objRef.left = objRef.top = '0px';
		else objRef.left = objRef.top =  -10000 + (!olNs4 ? 'px' : 0);
	}

	return divContainer;
}

// get reference to a layer with ID=id
function layerReference(id) {
	return (olNs4 ? o3_frame.document.layers[id] : (document.all ? o3_frame.document.all[id] : o3_frame.document.getElementById(id)));
}
////////
//  UTILITY FUNCTIONS
////////

// Checks if something is a function.
function isFunction(fnRef) {
	var rtn = true;

	if (typeof fnRef == 'object') {
		for (var i = 0; i < fnRef.length; i++) {
			if (typeof fnRef[i]=='function') continue;
			rtn = false;
			break;
		}
	} else if (typeof fnRef != 'function') {
		rtn = false;
	}
	
	return rtn;
}

// Converts an array into an argument string for use in eval.
function argToString(array, strtInd, argName) {
	var jS = strtInd, aS = '', ar = array;
	argName=(argName ? argName : 'ar');
	
	if (ar.length > jS) {
		for (var k = jS; k < ar.length; k++) aS += argName+'['+k+'], ';
		aS = aS.substring(0, aS.length-2);
	}
	
	return aS;
}

// Places a hook in the correct position in a hook point.
function reOrder(hookPt, fnRef, order) {
	var newPt = new Array(), match, i, j;

	if (!order || typeof order == 'undefined' || typeof order == 'number') return hookPt;
	
	if (typeof order=='function') {
		if (typeof fnRef=='object') {
			newPt = newPt.concat(fnRef);
		} else {
			newPt[newPt.length++]=fnRef;
		}
		
		for (i = 0; i < hookPt.length; i++) {
			match = false;
			if (typeof fnRef == 'function' && hookPt[i] == fnRef) {
				continue;
			} else {
				for(j = 0; j < fnRef.length; j++) if (hookPt[i] == fnRef[j]) {
					match = true;
					break;
				}
			}
			if (!match) newPt[newPt.length++] = hookPt[i];
		}

		newPt[newPt.length++] = order;

	} else if (typeof order == 'object') {
		if (typeof fnRef == 'object') {
			newPt = newPt.concat(fnRef);
		} else {
			newPt[newPt.length++] = fnRef;
		}
		
		for (j = 0; j < hookPt.length; j++) {
			match = false;
			if (typeof fnRef == 'function' && hookPt[j] == fnRef) {
				continue;
			} else {
				for (i = 0; i < fnRef.length; i++) if (hookPt[j] == fnRef[i]) {
					match = true;
					break;
				}
			}
			if (!match) newPt[newPt.length++]=hookPt[j];
		}

		for (i = 0; i < newPt.length; i++) hookPt[i] = newPt[i];
		newPt.length = 0;
		
		for (j = 0; j < hookPt.length; j++) {
			match = false;
			for (i = 0; i < order.length; i++) {
				if (hookPt[j] == order[i]) {
					match = true;
					break;
				}
			}
			if (!match) newPt[newPt.length++] = hookPt[j];
		}
		newPt = newPt.concat(order);
	}

	hookPt = newPt;

	return hookPt;
}

////////
//  PLUGIN ACTIVATION FUNCTIONS
////////

// Runs plugin functions to set runtime variables.
function setRunTimeVariables(){
	if (typeof runTime != 'undefined' && runTime.length) {
		for (var k = 0; k < runTime.length; k++) {
			runTime[k]();
		}
	}
}

// Runs plugin functions to parse commands.
function parseCmdLine(pf, i, args) {
	if (typeof cmdLine != 'undefined' && cmdLine.length) { 
		for (var k = 0; k < cmdLine.length; k++) { 
			var j = cmdLine[k](pf, i, args);
			if (j >- 1) {
				i = j;
				break;
			}
		}
	}

	return i;
}

// Runs plugin functions to do things after parse.
function postParseChecks(pf,args){
	if (typeof postParse != 'undefined' && postParse.length) {
		for (var k = 0; k < postParse.length; k++) {
			if (postParse[k](pf,args)) continue;
			return false;  // end now since have an error
		}
	}
	return true;
}


////////
//  PLUGIN REGISTRATION FUNCTIONS
////////

// Registers commands and creates constants.
function registerCommands(cmdStr) {
	if (typeof cmdStr!='string') return;

	var pM = cmdStr.split(',');
	pms = pms.concat(pM);

	for (var i = 0; i< pM.length; i++) {
		eval(pM[i].toUpperCase()+'='+pmCount++);
	}
}

// Registers no-parameter commands
function registerNoParameterCommands(cmdStr) {
	if (!cmdStr && typeof cmdStr != 'string') return;
	pmt=(!pmt) ? cmdStr : pmt + ',' + cmdStr;
}

// Register a function to hook at a certain point.
function registerHook(fnHookTo, fnRef, hookType, optPm) {
	var hookPt, last = typeof optPm;
	
	if (fnHookTo == 'plgIn'||fnHookTo == 'postParse') return;
	if (typeof hookPts[fnHookTo] == 'undefined') hookPts[fnHookTo] = new FunctionReference();

	hookPt = hookPts[fnHookTo];

	if (hookType != null) {
		if (hookType == FREPLACE) {
			hookPt.ovload = fnRef;  // replace normal overlib routine
			if (fnHookTo.indexOf('ol_content_') > -1) hookPt.alt[pms[CSSOFF-1-pmStart]]=fnRef; 

		} else if (hookType == FBEFORE || hookType == FAFTER) {
			var hookPt=(hookType == 1 ? hookPt.before : hookPt.after);

			if (typeof fnRef == 'object') {
				hookPt = hookPt.concat(fnRef);
			} else {
				hookPt[hookPt.length++] = fnRef;
			}

			if (optPm) hookPt = reOrder(hookPt, fnRef, optPm);

		} else if (hookType == FALTERNATE) {
			if (last=='number') hookPt.alt[pms[optPm-1-pmStart]] = fnRef;
		} else if (hookType == FCHAIN) {
			hookPt = hookPt.chain; 
			if (typeof fnRef=='object') hookPt=hookPt.concat(fnRef); // add other functions 
			else hookPt[hookPt.length++]=fnRef;
		}

		return;
	}
}

// Register a function that will set runtime variables.
function registerRunTimeFunction(fn) {
	if (isFunction(fn)) {
		if (typeof fn == 'object') {
			runTime = runTime.concat(fn);
		} else {
			runTime[runTime.length++] = fn;
		}
	}
}

// Register a function that will handle command parsing.
function registerCmdLineFunction(fn){
	if (isFunction(fn)) {
		if (typeof fn == 'object') {
			cmdLine = cmdLine.concat(fn);
		} else {
			cmdLine[cmdLine.length++] = fn;
		}
	}
}

// Register a function that does things after command parsing. 
function registerPostParseFunction(fn){
	if (isFunction(fn)) {
		if (typeof fn == 'object') {
			postParse = postParse.concat(fn);
		} else {
			postParse[postParse.length++] = fn;
		}
	}
}

////////
//  PLUGIN REGISTRATION FUNCTIONS
////////

// Runs any hooks registered.
function runHook(fnHookTo, hookType) {
	var l = hookPts[fnHookTo], k, rtnVal = null, optPm, arS, ar = runHook.arguments;

	if (hookType == FREPLACE) {
		arS = argToString(ar, 2);

		if (typeof l == 'undefined' || !(l = l.ovload)) rtnVal = eval(fnHookTo+'('+arS+')');
		else rtnVal = eval('l('+arS+')');

	} else if (hookType == FBEFORE || hookType == FAFTER) {
		if (typeof l != 'undefined') {
			l=(hookType == 1 ? l.before : l.after);
	
			if (l.length) {
				arS = argToString(ar, 2);
				for (var k = 0; k < l.length; k++) eval('l[k]('+arS+')');
			}
		}
	} else if (hookType == FALTERNATE) {
		optPm = ar[2];
		arS = argToString(ar, 3);

		if (typeof l == 'undefined' || (l = l.alt[pms[optPm-1-pmStart]]) == 'undefined') {
			rtnVal = eval(fnHookTo+'('+arS+')');
		} else {
			rtnVal = eval('l('+arS+')');
		}
	} else if (hookType == FCHAIN) {
		arS=argToString(ar,2);
		l=l.chain;

		for (k=l.length; k > 0; k--) if((rtnVal=eval('l[k-1]('+arS+')'))!=void(0)) break;
	}

	return rtnVal;
}

////////
// OBJECT CONSTRUCTORS
////////

// Object for handling hooks.
function FunctionReference() {
	this.ovload = null;
	this.before = new Array();
	this.after = new Array();
	this.alt = new Array();
	this.chain = new Array();
}

// Object for simple access to the overLIB version used.
// Examples: simpleversion:351 major:3 minor:5 revision:1
function Info(version, prerelease) {
	this.version = version;
	this.prerelease = prerelease;

	this.simpleversion = Math.round(this.version*100);
	this.major = parseInt(this.simpleversion / 100);
	this.minor = parseInt(this.simpleversion / 10) - this.major * 10;
	this.revision = parseInt(this.simpleversion) - this.major * 100 - this.minor * 10;
	this.meets = meets;
}

// checks for Core Version required
function meets(reqdVersion) {
	return (!reqdVersion) ? false : this.simpleversion >= Math.round(100*parseFloat(reqdVersion));
}


////////
// STANDARD REGISTRATIONS
////////
registerHook("ol_content_simple", ol_content_simple, FALTERNATE, CSSOFF);
registerHook("ol_content_caption", ol_content_caption, FALTERNATE, CSSOFF);
registerHook("ol_content_background", ol_content_background, FALTERNATE, CSSOFF);
registerHook("ol_content_simple", ol_content_simple, FALTERNATE, CSSCLASS);
registerHook("ol_content_caption", ol_content_caption, FALTERNATE, CSSCLASS);
registerHook("ol_content_background", ol_content_background, FALTERNATE, CSSCLASS);
registerPostParseFunction(checkPositionFlags);
registerHook("hideObject", nbspCleanup, FAFTER);
registerHook("horizontalPlacement", horizontalPlacement, FCHAIN);
registerHook("verticalPlacement", verticalPlacement, FCHAIN);
if (olNs4||(olIe5&&isMac)||olKq) olLoaded=1;
registerNoParameterCommands('sticky,autostatus,autostatuscap,fullhtml,hauto,vauto,closeclick,wrap,followmouse,mouseoff,compatmode');
///////
// ESTABLISH MOUSECAPTURING
///////

// Capture events, alt. diffuses the overlib function.
var olCheckMouseCapture=true;
if ((olNs4 || olNs6 || olIe4)) {
	olMouseCapture();
} else {
	overlib = no_overlib;
	nd = no_overlib;
	ver3fix = true;
}

// end of the overlib code

EOF
    ;



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

  my ($overlib_file) = "$FileStructure::TINDERBOX_HTML_DIR/OverLib.js";
  my ($overlib_link) = "$FileStructure::URL_HTML/OverLib.js";

  # The overlib code is 45K. Put it in its own file, which is not
  # overwritten, so that status pages load faster. Hopefuly browsers
  # will keep this file cached separately from the status page.

  if ( !(-e $overlib_file) || (-M $overlib_file > 1 ) ) {

      main::overwrite_file(
                           $overlib_file, 
                           $OVERLIB_JS
                           );
    }

$header .=<<EOF;
<HTML>
<HEAD>
	$refresh
        <TITLE>$args{'title'}</TITLE>
</HEAD>
<BODY TEXT=\"#000000\" BGCOLOR=\"#ffffff\">

<!-- This file was automatically created by $main::0  -->
<!-- version: $main::VERSION -->
<!-- at $main::LOCALTIME -->

<script language=\"JavaScript\">

var ol_fgcolor = \"#FFFF80\";
//ol_capcolor = \"#ffcc00\";
//var ol_textfont = \"Courier New, Courier\";
var ol_textsize = \"2\";
//var ol_sticky = 1;
var ol_vauto = 1;
var ol_hauto = 1;

</script>

<div id=\"overDiv\" style=\"position:absolute; visibility:hidden; z-index:1000;\">
</div>

<script language=\"JavaScript\" src=\"$overlib_link\">
</script>

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

  my $popup = '';
  
  # set the defaults

  $args{'windowtitle'} = $args{'windowtitle'} ||
      $DEFAULT_POPUP_TITLE;


  if ($args{'windowtxt'}) {
        
      # These characters inside the popupwindow will confuse my
      # popup window software into thinking the arguments are
      # over. I believe this is a ntescape specific problem.
      
      $args{'windowtxt'} =~ s/[\n\'\"]//g;
      $args{'windowtxt'} =~ s/[\n\t]+/ /g;
      $args{'windowtxt'} =~ s/\s+/ /g;
        
      push @POPUPTXT, $args{'windowtxt'};
      
      # overlib can be used like this:
      # onmouseover="return overlib('HTMLText');" onmouseout="return nd();"
      # onclick="return overlib('HTMLText');" onmouseout="return nd();"
      
      $popup .= "onClick=\" ";
      
      $popup .= "return overlib(popuptxt[$#POPUPTXT]";
      
      if($args{'windowtitle'})
      {
          $popup .= ",CAPTION,\'$args{'windowtitle'}\'";
      }

      # The window needs to be sticky because the windows might
      # contain links and mouse movement should not close the windows
      # or the links will be impossible to press.

      $popup .= ",STICKY";
      
      $popup .= "); \" ";
      
      $popup .= "onMouseOut=\" return nd(); \" ";
  }
  

  my $name = '';
  if ($args{'name'}) {
      $name = "name=\"$args{'name'}\"";
  }
  
  my $linktxt =  $args{'linktxt'} || '';
  
  my $href = '';

  # I would like to support the use of text only browsers who can not
  # render the javascript popup to click through. But I can not find a
  # way to have both the popups for those browsers who can render them
  # and the link for the others.
  #
  #  $href .= "HREF=\"$args{'href'}\"";

  if ($args{'windowtxt'}) {
      $href .= "HREF=\"javascript:void(0);\"";
  } elsif ($args{'href'}) {
      $href .= "HREF=\"$args{'href'}\"";
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

