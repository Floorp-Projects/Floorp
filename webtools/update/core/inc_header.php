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

    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
	<meta name="keywords" content="mozilla update, mozilla extensions, mozilla plugins, thunderbird themes, thunderbird extensions, firefox extensions, firefox themes,">
	
	<link rel="stylesheet" type="text/css" href="/css/print.css" media="print">
	
	<link rel="stylesheet" type="text/css" href="/css/base/content.css" media="all">
	<link rel="stylesheet" type="text/css" href="/css/cavendish/content.css" title="Cavendish" media="all">
	
	<link rel="stylesheet" type="text/css" href="/css/base/template.css" media="screen">
	<link rel="stylesheet" type="text/css" href="/css/cavendish/template.css" title="Cavendish" media="screen">
	
	<link rel="icon" href="/images/favicon.png" type="image/png">
	
	<link rel="home" title="Home" href="http://update.mozilla.org/">
</head>

<body id="update-mozilla-org">
<div id="container">

<p class="skipLink"><a href="#firefox-feature" accesskey="2">Skip to main content</a></p>

<div id="mozilla-org"><a href="http://www.mozilla.org/">Visit Mozilla.org</a></div>
<div id="header">
	
	<div id="key-title">
	<h1><a href="/" title="Return to home page" accesskey="1">Mozilla Update: Beta</a></h1>
	<ul>
		<li><a href="/" title="Learn More About Mozilla Updates">home</a></li>
		<li><a href="/about/" title="Learn More About Mozilla Updates">about</a></li>
		<li><a href="/developers/" title="Find Tools and Information for Developers">developers</a></li>
		<li>
		<form id="search" method="get" action="/quicksearch.php" title="Search Mozilla Update">
		<div>
		<label for="q" title="Search Mozilla Update">search:</label>
		<input type="text" id="q" name="q" accesskey="s" size="10">
		<select name="section" id="sectionsearch">
		  <option value="A">Entire Site</option>
		  <option value="E">Extensions</option>
		  <option value="T">Themes</option>
		<!--
		  <option value="P">Plugins</option>
		  <option value="S">Search Engines</option>
		-->
		</select>
		<input type="submit" id="submit" value="Go">
		</div>
		</form>
		</li>
	</ul>
	</div>
    <?php
    $uriparams_skip="application";
    ?>
	<div id="key-menu">	
		<dl id="menu-firefox">
		<dt>Firefox:</dt>
		<dd><a href="/extensions/?<?php echo"".uriparams()."&amp;"; ?>application=firefox" title="Get Extensions for the Firefox Browser">Extensions</a>, <a href="/themes/?<?php echo"".uriparams()."&amp;"; ?>application=firefox" title="Get Themes for the Firefox Browser">Themes</a>, <a href="/plugins/" title="Get Plugins for Firefox">Plugins</a><!--, <a href="/searchengines/" title="Get New Search Engines for the Search Box in Firefox">Search Engines</a>--></dd>
		</dl>
		<dl id="menu-thunderbird">
		<dt>Thunderbird:</dt>
		<dd><a href="/extensions/?<?php echo"".uriparams()."&amp;"; ?>application=thunderbird" title="Get Extensions for Thunderbird Email">Extensions</a>, <a href="/themes/?<?php echo"".uriparams()."&amp;"; ?>application=thunderbird" title="Get Themes for Thunderbird Email">Themes</a></dd>
		</dl>
		<dl id="menu-mozillasuite">
		<dt>Mozilla Suite:</dt>
		<dd><a href="/extensions/?<?php echo"".uriparams()."&amp;"; ?>application=mozilla" title="Get Extensions for the Mozilla Suite">Extensions</a>, <a href="/themes/?<?php echo"".uriparams()."&amp;"; ?>application=mozilla" title="Get Themes for the Mozilla Suite">Themes</a>, <a href="/plugins/" title="Get Plugins for Mozilla Suite">Plugins</a></dd>
		</dl>
		<div class="ie-clear-menu">&nbsp;</div>
	</div>
    <?php
    unset($uriparams_skip);
    ?>

</div>
<!-- closes #header-->

<hr class="hide">