<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html lang="en">

<head>
 <title>{$title}</title>
 <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">

 <link rel="stylesheet" type="text/css" href="http://mozilla.org/css/print.css" media="print">
 <link rel="stylesheet" type="text/css" href="http://mozilla.org/css/base/content.css" media="all">
 <link rel="stylesheet" type="text/css" href="http://mozilla.org/css/cavendish/content.css" title="Cavendish" media="all">

 <link rel="stylesheet" type="text/css" href="http://mozilla.org/css/base/template.css" media="screen">
 <link rel="stylesheet" type="text/css" href="http://mozilla.org/css/cavendish/template.css" title="Cavendish" media="screen">

 <link rel="stylesheet" type="text/css" href="{$base_url}/app/style.css" media="all">

 <link rel="icon" href="images/mozilla-16.png" type="image/png">

 <link rel="home" title="Home" href="http://www.mozilla.org/">
</head>

<body id="www-mozilla-org" class="homepage">
<div id="container">

<p class="skipLink"><a href="#firefox-feature" accesskey="2">Skip to main content</a></p>

<div id="header">
 <h1><a href="/" title="Return to home page" accesskey="1">Mozilla</a></h1>
 <ul>
  <li id="login"><a href="{$base_url}/app/login" title="Admin Login">Login</a></li>
  <li id="stats"><a href="{$base_url}/app/stats/" title="View Statistics">Stats</a></li>
  <li id="top_25"><a href="{$base_url}/app/query/?show=25&count=on&&submit_query=Search" title="Top 25 Hosts">Top 25</a></li>
  <li id="query"><a href="{$base_url}/app" title="Create a new Query">Query</a></li>
 </ul>
 <form id="search" method="get" action="{$base_url}/app/report/" title="Get Report">
 <div>

  <label for="report_id" title="Pull up report number">get report:</label>
  <input type="text" id="report_id" name="report_id" accesskey="g" size="30">
  <input type="submit" id="submit" value="Lookup">
 </div>
 </form>

</div>
<!-- closes #header-->
<h1>Mozilla Reporter</h1>
<!--
reporter.mozilla.org by:
  Robert "DIGITALgimpus" Accettura <http://robert.accettura.com>
-->
{$content}
  <hr class="hide">
  <div id="footer">
   <ul id="bn">
    <li><a href="http://planet.mozilla.org">Community Blogs</a></li>
    <li><a href="http://www.mozilla.org/sitemap.html">Site Map</a></li>
    <li><a href="http://www.mozilla.org/security/">Security Updates</a></li>

    <li><a href="http://www.mozilla.org/contact/">Contact Us</a></li>
    <li><a href="http://www.mozilla.org/foundation/donate.html">Donate</a></li>
   </ul>
   <p>International Affiliates: <a href="http://www.mozilla-europe.org/">Mozilla Europe</a> - <a href="http://www.mozilla-japan.org/">Mozilla Japan</a></p>
   <p>Copyright &copy; 1998-<?php print date("Y"); ?> The Mozilla Organization</p>

  </div>
  <!-- closes #footer-->

</div>
<!-- closes #container -->

</body>
</html>
