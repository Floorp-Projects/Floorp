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

$pos = strpos($_SERVER["REQUEST_URI"], "/admin");
if ($pos !== false) {
echo'<LINK REL="STYLESHEET" TYPE="text/css" HREF="/core/mozupdates.bak.css">';
$application="login"; $_SESSION["application"]="login"; unset($_SESSION["app_version"], $_SESSION["app_os"]);
 }
?>
<DIV class="header">
<?php //if ($_GET["application"]) {$application=$_GET["application"]; } else {$application="firefox"; } ?>
<DIV class="logo"><IMG SRC="/images/<?php echo"$application"; ?>-cornerlogo.png" BORDER=0 ALT=""></DIV>
<DIV class="header-top"><A HREF="/"><IMG SRC="/images/updatelogo.png" BORDER=0 HEIGHT=55 WIDTH=270 ALT="Mozilla Update"></A></DIV>
<DIV class="tabbar">
<A HREF="/?application=mozilla"><IMG SRC="/images/tab-mozilla<?php if ($application=="mozilla") {echo"-selected"; } ?>.png" BORDER=0 HEIGHT=20 WIDTH=98 ALT="[Mozilla] "></A><A HREF="/?application=firefox"><IMG SRC="/images/tab-firefox<?php if ($application=="firefox") {echo"-selected"; } ?>.png" BORDER=0 HEIGHT=20 WIDTH=98 ALT="[Firefox] "></A><A HREF="/?application=thunderbird"><IMG SRC="/images/tab-thunderbird<?php if ($application=="thunderbird") {echo"-selected"; } ?>.png" BORDER=0 HEIGHT=20 WIDTH=105 ALT="[Thunderbird] "></A><A HREF="/developercp.php"><IMG SRC="/images/tab-login<?php if ($application=="login") {echo"-selected"; } ?>.png" BORDER=0 HEIGHT=20 WIDTH=98 ALT="[Login]"></A>
</DIV>
</DIV>
<DIV class="bar"></DIV>
<DIV class="nav">
<?php if ($application !=="login") { ?>
<A HREF="/">Home</A>
 | <A HREF="/faq/">FAQ</A>
<?php
// Types
$types = array("E"=>"Extensions","T"=>"Themes");
foreach($types as $typeid => $typename) {
echo" | <A HREF=\"/".strtolower($typename)."/\">$typename</A>";
}
//echo"<BR>\n";

} else { echo"&nbsp;"; }
?>
</DIV>

<?php
if ($pos !== false) {
?>
<?php if ($_SESSION["logoncheck"] =="YES" && $headerline !="none") { ?>
<DIV class="adminheading">
Welcome<?php echo" $_SESSION[name]"; ?>!
<a href="usermanager.php?function=edituser&amp;userid=<?php echo"$_SESSION[uid]"; ?>">Your Profile</a> | <A HREF="main.php">Home</A> |<A HREF="logout.php">Logout</A>
<?php } ?>
</DIV>
<div id="mainContent">
<?php
}
?>