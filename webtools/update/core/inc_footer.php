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
<!--Page Footer-->
  <hr class="hide">
  <div id="footer">
   <ul id="bn">
    <li><a href="/about/policies/">Terms of Use</a></li>
    <li><a href="/about/contact/">Contact Us</a></li>
    <li><a href="http://www.mozilla.org/foundation/donate.html">Donate</a></li>
   </ul>
    <p><strong>Update-Beta is a technology Preview. For internal use only.</strong><br>
    Copyright &copy; 2004 The Mozilla Organization</p>
  </div>
  <!-- closes #footer-->

</div>
<!-- closes #container -->

<?php

return;

//Site Timer Counter :: Debug-Mode Item Only
$time_end = getmicrotime();
//Returns in format: sss.mmmuuunnnppp ;-) 
// m = millisec, u=microsec, n=nansec, p=picosec
$time = round($time_end - $time_start,"6");

echo"<DIV class=\"footer\">&copy; 2004 <A HREF=\"http://www.mozilla.org\">The Mozilla Organization</A>"; if ($_SESSION["debug"]=="true") {echo" | Page Created in $time seconds"; } echo" | Terms of Use | Top</DIV>"; //Debug Time

if ($pos !== false) {
echo"</div>\n";
}
?>
