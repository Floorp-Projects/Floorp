<?php
// ***** BEGIN LICENSE BLOCK *****
// Version: MPL 1.1/GPL 2.0/LGPL 2.1
//
// The contents of this file are subject to the Mozilla Public License Version
// 1.1 (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
// http://www.mozilla.org/MPL/
//
// Software distributed under the License is distributed on an "AS IS" basis,
// WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
// for the specific language governing rights and limitations under the
// License.
//
// The Original Code is Mozilla Update.
//
// The Initial Developer of the Original Code is
// Chris "Wolf" Crews.
// Portions created by the Initial Developer are Copyright (C) 2004
// the Initial Developer. All Rights Reserved.
//
// Contributor(s):
//   Chris "Wolf" Crews <psychoticwolf@carolina.rr.com>
//
// Alternatively, the contents of this file may be used under the terms of
// either the GNU General Public License Version 2 or later (the "GPL"), or
// the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
// in which case the provisions of the GPL or the LGPL are applicable instead
// of those above. If you wish to allow use of your version of this file only
// under the terms of either the GPL or the LGPL, and not to allow others to
// use your version of this file under the terms of the MPL, indicate your
// decision by deleting the provisions above and replace them with the notice
// and other provisions required by the GPL or the LGPL. If you do not delete
// the provisions above, a recipient may use your version of this file under
// the terms of any one of the MPL, the GPL or the LGPL.
//
// ***** END LICENSE BLOCK *****
?>
<?php
require"../core/config.php";
?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html lang="en">

<head>
    <title>Mozilla Update :: Plugins</title>

<?php
include"$page_header";
?>

<div id="mBody">

    <div id="side">
    <ul id="nav">
        <li><a href="http://plugindoc.mozdev.org" title="Plugin Support for Mozilla-based Browsers">Plugin Documentation</a></li>
        <li><a href="/">Mozilla Update Home</a></li>
    </ul>
    </div>
	<div id="mainContent">

<h2>Common Plugins for Mozilla Firefox and Mozilla Suite</h2>
<p class="first">Plugins are programs that allow websites to provide content to you and have it appear in your browser.</p>


<DIV class="item">
    <h2 class="first"><A HREF="http://www.adobe.com/products/acrobat/readermain.html">Acrobat Reader</A></h2>
    <P class="first">By <A HREF="http://www.adobe.com">Adobe Systems</A></P>
    <p class="first">For viewing and printing Adobe Portable Document Format (PDF) files</p>
    <p><img src="/images/faq_small.png" style="float: left;" height=16 width=16 alt="">&nbsp;Adobe Reader 7.0 requires Windows 2000 or later. If you are using Windows 98 SE, Windows Me, or Windows NT 4.0, you will need to use Adobe Reader 6.0.3</p>
    <h3><IMG SRC="/images/install.png" HEIGHT=24 WIDTH=24 ALT="">&nbsp;Download</h3>
    <DIV style="margin-top: 5px; height: 50px">
        <DIV class="iconbar"><IMG SRC="/images/windows_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For Windows:<BR>&nbsp;&nbsp;<a href="http://ardownload.adobe.com/pub/adobe/reader/win/7x/7.0/enu/AdbeRdr70_enu_full.exe">Version 7.0</a><br><a href="http://ardownload.adobe.com/pub/adobe/reader/win/6.x/6.0/enu/AdbeRdr602_distrib_enu.exe">6.02 Full</a> / <a href="http://ardownload.adobe.com/pub/adobe/acrobat/win/6.x/6.0.3/misc/Acro-Reader_603_Update.exe">6.03 Update</a></DIV>
        <DIV class="iconbar"><IMG SRC="/images/linux_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For Linux:<BR>&nbsp;&nbsp;<a href="http://ardownload.adobe.com/pub/adobe/acrobatreader/unix/5.x/linux-5010.tar.gz">Version 5.10</a></DIV>
        <DIV class="iconbar"><IMG SRC="/images/macosx_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For MacOSX:<BR>&nbsp;&nbsp;Unsupported <a href="http://plugindoc.mozdev.org/OSX.html#Acrobat">(Details)</a></DIV>
    </DIV>
    <DIV class="baseline"><img src="/images/faq_small.png" style="float: left;" height=16 width=16 alt="">&nbsp;Support Documentation: <a href="http://plugindoc.mozdev.org/windows.html#Acrobat">Windows</a>, <a href="http://plugindoc.mozdev.org/linux.html#Acrobat">Linux</a>, <a href="http://plugindoc.mozdev.org/OSX.html#Acrobat">MacOSX</a> | <a href="http://plugindoc.mozdev.org/faqs/acroread.html">Acrobat Reader FAQ</a></DIV>
</DIV>

<DIV class="item">
    <h2 class="first"><A HREF="http://www.macromedia.com/software/flashplayer/">Flash Player</A></h2>
    <P class="first">By <A HREF="http://www.macromedia.com">Macromedia</A></P>
    <p class="first">Macromedia Flash Player is the universal rich client for delivering effective Macromedia Flash experiences across desktops and devices.</p>
    <h3><IMG SRC="/images/install.png" HEIGHT=24 WIDTH=24 ALT="">&nbsp;Download</h3>
    <DIV style="margin-top: 5px; height: 38px">
        <DIV class="iconbar"><IMG SRC="/images/windows_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For Windows:<BR>&nbsp;&nbsp;<a href="http://www.macromedia.com/shockwave/download/index.cgi?P1_Prod_Version=ShockwaveFlash&amp;P2_Platform=Win32&amp;P3_Browser_Version=NetscapePre4">Version 7.0</a></DIV>
        <DIV class="iconbar"><IMG SRC="/images/linux_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For Linux:<BR>&nbsp;&nbsp;<a href="http://www.macromedia.com/shockwave/download/download.cgi?P1_Prod_Version=ShockwaveFlash&amp;P2_Platform=Linux&amp;P3_Browser_Version=Netscape4">Version 7.0</a></DIV>
        <DIV class="iconbar"><IMG SRC="/images/macosx_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For MacOSX:<BR>&nbsp;&nbsp;<a href="http://www.macromedia.com/shockwave/download/index.cgi?P1_Prod_Version=ShockwaveFlash&amp;P2_Platform=MacOSX">Version 7.0</a></DIV>
    </DIV>
    <DIV class="baseline"><img src="/images/faq_small.png" style="float: left;" height=16 width=16 alt="">&nbsp;Support Documentation: <a href="http://plugindoc.mozdev.org/windows.html#Flash">Windows</a>, <a href="http://plugindoc.mozdev.org/linux.html#Flash">Linux</a>, <a href="http://plugindoc.mozdev.org/OSX.html#Flash">MacOSX</a> | <a href="http://plugindoc.mozdev.org/faqs/flash.html">Flash Player FAQ</a></DIV>
</DIV>

<DIV class="item">
    <h2 class="first"><A HREF="http://www.java.com/en/about/">Java</A></h2>
    <P class="first">By <A HREF="http://www.sun.com">Sun Microsystems</A></P>
    <p class="first">The Java Runtime Enviroment enables your computer to run applications and applets that use Java technology.</p>
    <h3><IMG SRC="/images/install.png" HEIGHT=24 WIDTH=24 ALT="">&nbsp;Download</h3>
    <DIV style="margin-top: 5px; height: 38px">
        <DIV class="iconbar"><IMG SRC="/images/windows_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For Windows:<BR>&nbsp;&nbsp;<a href="http://javashoplm.sun.com/ECom/docs/Welcome.jsp?StoreId=22&amp;PartDetailId=jre-1.5.0_01-oth-JPR&amp;SiteId=JSC&amp;TransactionId=noreg">Version 5.0 Update 1</a></DIV>
        <DIV class="iconbar"><IMG SRC="/images/linux_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For Linux:<BR>&nbsp;&nbsp;<a href="http://javashoplm.sun.com/ECom/docs/Welcome.jsp?StoreId=22&amp;PartDetailId=jre-1.5.0_01-oth-JPR&amp;SiteId=JSC&amp;TransactionId=noreg">Version 5.0 Update 1</a></DIV>
        <DIV class="iconbar"><IMG SRC="/images/macosx_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For MacOSX:<BR>&nbsp;&nbsp;<a href="http://plugindoc.mozdev.org/OSX.html#Java">Details</a></DIV>
    </DIV>
    <DIV class="baseline"><img src="/images/faq_small.png" style="float: left;" height=16 width=16 alt="">&nbsp;Support Documentation: <a href="http://plugindoc.mozdev.org/windows.html#Java">Windows</a>, <a href="http://plugindoc.mozdev.org/linux.html#Java">Linux</a>, <a href="http://plugindoc.mozdev.org/OSX.html#Java">MacOSX</a> | <a href="http://plugindoc.mozdev.org/faqs/java.html">Java Plugin FAQ</a></DIV>
</DIV>


<DIV class="item">
    <h2 class="first"><A HREF="http://www.apple.com/quicktime/">Quicktime</A></h2>
    <P class="first">By <A HREF="http://www.apple.com">Apple Computer</A></P>
    <p class="first">QuickTime Player is an easy-to-use application for playing, interacting with or viewing video, audio, VR or graphics files.</p>
    <h3><IMG SRC="/images/install.png" HEIGHT=24 WIDTH=24 ALT="">&nbsp;Download</h3>
    <DIV style="margin-top: 5px; height: 38px">
        <DIV class="iconbar"><IMG SRC="/images/windows_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For Windows:<BR>&nbsp;&nbsp;<a href="http://www.apple.com/quicktime/download/">Version 6.5.2</a></DIV>
        <DIV class="iconbar"><IMG SRC="/images/linux_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For Linux:<BR>&nbsp;&nbsp;Unavailable</DIV>
        <DIV class="iconbar"><IMG SRC="/images/macosx_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For MacOSX:<BR>&nbsp;&nbsp;<a href="http://www.apple.com/quicktime/download/">Version 6.5.2</a></DIV>
    </DIV>
    <DIV class="baseline"><img src="/images/faq_small.png" style="float: left;" height=16 width=16 alt="">&nbsp;Support Documentation: <a href="http://plugindoc.mozdev.org/windows.html#Quicktime">Windows</a>, <a href="http://plugindoc.mozdev.org/OSX.html#Quicktime">MacOSX</a></DIV>
</DIV>

<DIV class="item">
    <h2 class="first"><A HREF="http://www.real.com/player/">RealPlayer</A></h2>
    <P class="first">By <A HREF="http://www.real.com">Real Networks</A></P>
    <p class="first">RealPlayer enables your computer to play streaming RealVideo and RealAudio.</p>
    <h3><IMG SRC="/images/install.png" HEIGHT=24 WIDTH=24 ALT="">&nbsp;Download</h3>
    <DIV style="margin-top: 5px; height: 38px">
        <DIV class="iconbar"><IMG SRC="/images/windows_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For Windows:<BR>&nbsp;&nbsp;<a href="http://www.real.com/R/RC.040104freeplayer.def...R/forms.real.com/real/realone/realone.html?type=eva1&amp;rppr=rnwk&amp;src=040104freeplayer">Version 10</a></DIV>
        <DIV class="iconbar"><IMG SRC="/images/linux_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For Linux:<BR>&nbsp;&nbsp;<a href="http://www.real.com/R/RC.040104freeplayer.def...R/forms.real.com/real/player/unix/unix.html?rppr=rnwk&amp;src=040104freeplayer">Version 10</a></DIV>
        <DIV class="iconbar"><IMG SRC="/images/macosx_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For MacOSX:<BR>&nbsp;&nbsp;<a href="http://www.real.com/R/RC.040104freeplayer.def...R/forms.real.com/real/realone/mac.html?rppr=rnwk&amp;src=040104freeplayer">Version 10</a></DIV>
    </DIV>
    <DIV class="baseline"><img src="/images/faq_small.png" style="float: left;" height=16 width=16 alt="">&nbsp;Support Documentation: <a href="http://plugindoc.mozdev.org/windows.html#RealOne">Windows</a>, <a href="http://plugindoc.mozdev.org/linux.html#RealPlayer">Linux</a>, <a href="http://plugindoc.mozdev.org/OSX.html#RealOne">MacOSX</a></DIV>
</DIV>

<DIV class="item">
    <h2 class="first"><A HREF="http://www.macromedia.com/software/shockwaveplayer/">Shockwave</A></h2>
    <P class="first">By <A HREF="http://www.macromedia.com">Macromedia</A></P>
    <p class="first">Shockwave Player displays Web content that has been created by Macromedia Director.</p>
    <h3><IMG SRC="/images/install.png" HEIGHT=24 WIDTH=24 ALT="">&nbsp;Download</h3>
    <DIV style="margin-top: 5px; height: 38px">
        <DIV class="iconbar"><IMG SRC="/images/windows_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For Windows:<BR>&nbsp;&nbsp;<a href="http://www.macromedia.com/shockwave/download/index.cgi?P1_Prod_Version=Shockwave&amp;P2_Platform=Win32&amp;P3_Browser_Version=NetscapePre4">Version 10</a></DIV>
        <DIV class="iconbar"><IMG SRC="/images/linux_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For Linux:<BR>&nbsp;&nbsp;Unavailable</DIV>
        <DIV class="iconbar"><IMG SRC="/images/macosx_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For MacOSX:<BR>&nbsp;&nbsp;<a href="http://www.macromedia.com/shockwave/download/index.cgi?P1_Prod_Version=Shockwave&amp;P2_Platform=MacOSX">Version 10</a></DIV>
    </DIV>
    <DIV class="baseline"><img src="/images/faq_small.png" style="float: left;" height=16 width=16 alt="">&nbsp;Support Documentation: <a href="http://plugindoc.mozdev.org/windows.html#Shockwave">Windows</a>, <a href="http://plugindoc.mozdev.org/OSX.html#Shockwave">MacOSX</a></DIV>
</DIV>

<DIV class="item">
    <h2 class="first"><A HREF="http://www.microsoft.com/windows/windowsmedia/default.aspx">Windows Media Player</A></h2>
    <P class="first">By <A HREF="http://www.microsoft.com">Microsoft</A></P>
    <p class="first">Windows Media Player lets you play streaming audio, video, animations, and multimedia presentations on the web.</p>
    <h3><IMG SRC="/images/install.png" HEIGHT=24 WIDTH=24 ALT="">&nbsp;Download</h3>
    <DIV style="margin-top: 5px; height: 38px">
        <DIV class="iconbar"><IMG SRC="/images/windows_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For Windows:<BR>&nbsp;&nbsp;<a href="http://www.microsoft.com/windows/windowsmedia/mp10/default.aspx">Version 10 for Windows XP</a>, <a href="http://www.microsoft.com/windows/windowsmedia/9series/player.aspx">Version 9</a></DIV>
        <DIV class="iconbar"><IMG SRC="/images/linux_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For Linux:<BR>&nbsp;&nbsp;Unavailable</DIV>
        <DIV class="iconbar"><IMG SRC="/images/macosx_icon.png" HEIGHT=34 WIDTH=34 ALT="">&nbsp;For MacOSX:<BR>&nbsp;&nbsp;<a href="http://www.microsoft.com/mac/otherproducts/otherproducts.aspx?pid=windowsmedia">Version 9</a></DIV>
    </DIV>
    <DIV class="baseline"><img src="/images/faq_small.png" style="float: left;" height=16 width=16 alt="">&nbsp;Support Documentation: <a href="http://plugindoc.mozdev.org/windows.html#WMP">Windows</a>, <a href="http://plugindoc.mozdev.org/OSX.html#WMP">MacOSX</a></DIV>
</DIV>

<h2>Looking for a plugin not listed here?</h2>
<p class="first">This page only lists the most common and most popular plugins. For more information about other plugins available for Mozilla-based Browsers, visit <a href="http://plugindoc.mozdev.org">PluginDoc</a>.</p>

</div>

</div>
<?php
include"$page_footer";
?>
</BODY>
</HTML>
