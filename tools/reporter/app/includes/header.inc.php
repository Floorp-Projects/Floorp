<?php
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Reporter (r.m.o).
 *
 * The Initial Developer of the Original Code is
 *      Robert Accettura <robert@accettura.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
printheaders();
header('Content-Type: text/html; charset=utf-8');
?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html lang="en">

<head>
 <title><?php print $title; ?></title>
 <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">

 <link rel="stylesheet" type="text/css" href="http://mozilla.org/css/print.css" media="print">
 <link rel="stylesheet" type="text/css" href="http://mozilla.org/css/base/content.css" media="all">
 <link rel="stylesheet" type="text/css" href="http://mozilla.org/css/cavendish/content.css" title="Cavendish" media="all">

 <link rel="stylesheet" type="text/css" href="http://mozilla.org/css/base/template.css" media="screen">
 <link rel="stylesheet" type="text/css" href="http://mozilla.org/css/cavendish/template.css" title="Cavendish" media="screen">

 <link rel="stylesheet" type="text/css" href="<?php print $config['app_url']; ?>/style.css" media="all">

 <link rel="icon" href="images/mozilla-16.png" type="image/png">

 <link rel="alternate" type="application/rss+xml" title="Mozilla Announcements" href="http://www.mozilla.org/news.rdf">
 <link rel="alternate" type="application/rss+xml" title="Mozilla Weblogs" href="http://planet.mozilla.org/rss10.xml">
 <link rel="alternate" type="application/rss+xml" title="mozillaZine News" href="http://www.mozillazine.org/atom.xml">

 <link rel="home" title="Home" href="http://www.mozilla.org/">
</head>

<body id="www-mozilla-org" class="homepage">
<div id="container">

<p class="skipLink"><a href="#firefox-feature" accesskey="2">Skip to main content</a></p>

<div id="header">
 <h1><a href="/" title="Return to home page" accesskey="1">Mozilla</a></h1>
 <ul>
  <li id="menu_aboutus"><a href="about/" title="Getting the most out of your online experience">About</a></li>

  <li id="menu_developers"><a href="developer/" title="Using Mozilla's products for your own applications">Developers</a></li>
  <li id="menu_store"><a href="http://www.mozillastore.com/?r=mozorg1" title="Shop for Mozilla products on CD and other merchandise">Store</a></li>
  <li id="menu_support"><a href="support/" title="Installation, trouble-shooting, and the knowledge base">Support</a></li>
  <li id="menu_products"><a href="products/" title="All software Mozilla currently offers">Products</a></li>
 </ul>
 <form id="search" method="get" action="http://www.google.com/custom" title="Mozilla.org Search">
 <div>

  <label for="q" title="Search mozilla.org&quot;s sites">search mozilla:</label>
  <input type="hidden" name="cof" value="LW:174;LH:60;L:http://www.mozilla.org/images/mlogosm.gif;GIMP:#cc0000;T:black;ALC:#0000ff;GFNT:grey;LC:#990000;BGC:white;AH:center;VLC:purple;GL:0;GALT:#666633;AWFID:9262c37cefe23a86;">
  <input type="hidden" name="domains" value="mozilla.org">
  <input type="hidden" name="sitesearch" value="mozilla.org">
  <input type="text" id="q" name="q" accesskey="s" size="30">
  <input type="submit" id="submit" value="Go">
 </div>
 </form>

</div>
<!-- closes #header-->
<h1>Mozilla Reporter</h1>
<!--
reporter.mozilla.org by:
  Robert "DIGITALgimpus" Accettura <http://robert.accettura.com>
-->
