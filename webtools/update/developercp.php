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
require"core/config.php";
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html401/loose.dtd">
<html lang="EN" dir="ltr">
<head>
    <meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
    <meta http-equiv="Content-Language" content="en">
<TITLE>Mozilla Update :: Developer Control Panel - Coming Soon</TITLE>

<LINK REL="STYLESHEET" TYPE="text/css" HREF="/core/update.css">
</HEAD>
<BODY>
<?php
$application="login"; $_SESSION["application"]="login"; unset($_SESSION["app_version"], $_SESSION["app_os"]);
include"$page_header";
?>
<DIV class="faqtitle">Developer Control Panel - Coming Soon</DIV>

<DIV style="width: 75%; margin: auto; margin-top: 20px; border: 1px dotted #0065CA; padding: 5px">
By Firefox 1.0, Mozilla Update will have a new section for Extension/Theme authors to login to. This new section
will give them the ability to manage their extensions and themes that're hosted on Mozilla Update themselves.<BR>
<BR>
Authors will have the ability to add a new extension or theme themselves, as well as adding new versions of that extension or theme,
updating an existing listing (including being able to change the application-compatibility settings w/o needing a new file), and if
they so desire, the ability to remove entirely an old version of an extension or theme or the entire listing.<BR>
<BR>
Other features that will be available include the capability of adding and managing preview images of their extension or theme,
a developer comments feature, so they can easily communicate to end-users known problems with the extension/theme.
<BR>
</DIV>

&nbsp;<P>

<?php
include"$page_footer";
?>
</BODY>
</HTML>