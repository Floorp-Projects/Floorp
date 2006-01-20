<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=ISO-8859-1">
<link rel="top" title="Home" href="http://www.mozilla.org/">
<link rel="stylesheet" type="text/css" href="{$base_url}/styles/theme/mozilla/print.css"  media="print">
<link rel="stylesheet" type="text/css" href="{$base_url}/styles/theme/mozilla/base/content.css"  media="all">
<link rel="stylesheet" type="text/css" href="{$base_url}/styles/theme/mozilla/cavendish/content.css" title="Cavendish" media="screen">
<link rel="stylesheet" type="text/css" href="{$base_url}/styles/theme/mozilla/base/template.css"  media="screen">
<link rel="stylesheet" type="text/css" href="{$base_url}/styles/theme/mozilla/cavendish/template.css" title="Cavendish" media="screen">
<link rel="stylesheet" type="text/css" href="{$base_url}/styles/theme/mozilla/reporter.css" title="Cavendish" media="screen">

<link rel="icon" href="http://www.mozilla.org/images/mozilla-16.png" type="image/png">

<title>{$title}</title>

<meta name="robots" content="all">
<meta name="author" content="Robert Accettura http://robert.accettura.com/ design by http://www.mezzoblue.com/">
<meta name="keywords" content="mozilla, reporter, broken website">

<link rel="author" href="http://robert.accettura.com/">
<link rel="designer" href="http://www.mezzoblue.com/">

</head>
<body id="www-mozilla-org" class="secondLevel reporter">
<div id="content">
<p class="skipLink"><a href="#mainContent" accesskey="2">Skip to main content</a></p>
<div id="mozilla-org"><a href="http://mozilla.org/">Visit mozilla.org</a></div>

<div id="header">
<h1><a href="{$base_url}/app" title="Return to home page" accesskey="1">Mozilla</a></h1>

<ul>
     <li id="login">{strip}
         {if $is_admin == true}
             <a href="{$base_url}/app/logout" title="Admin Logout">Logout</a>
         {else}
             <a href="{$base_url}/app/login" title="Admin Login">Login</a>
         {/if}
     {/strip}</li>
     <li id="stats"><a href="{$base_url}/app/stats/" title="View Statistics">Stats</a></li>
     <li id="top_25"><a href="{$base_url}/app/query/?show=25&amp;count=on&amp;submit_query=Search" title="Top 25 Hosts">Top 25</a></li>
     <li id="query"><a href="{$base_url}/app" title="Create a new Query">Query</a></li>
</ul>
<form id="search" method="get" action="http://www.google.com/custom" title="mozilla.org Search">
<div>
<label for="q" title="Search mozilla.org's sites">search mozilla:</label>
<input type="hidden" name="cof" value="LW:174;LH:60;L:http://www.mozilla.org/images/mlogosm.gif;GIMP:#cc0000;T:black;ALC:#0000ff;GFNT:grey;LC:#990000;BGC:white;AH:center;VLC:purple;GL:0;GALT:#666633;AWFID:9262c37cefe23a86;">

<input type="hidden" name="domains" value="mozilla.org">
<input type="hidden" name="sitesearch" value="mozilla.org">
<input type="text" id="q" name="q" accesskey="s" size="30">
<input type="submit" id="submit" value="Go">
</div>
</form>
</div>
<hr class="hide">
<div id="mBody">
<hr class="hide">
<div>

  <h1>Mozilla Reporter</h1>

    <!--
    reporter.mozilla.org by:
      Robert Accettura <http://robert.accettura.com>
    -->
    <div id="reporter-content">
    {$content}
    </div>

<hr class="hide">
</div>
</div>
<div id="footer">
<ul>
<li><a href="../sitemap.html">Site Map</a></li>
<li><a href="../security/">Security Updates</a></li>
<li><a href="../contact/">Contact Us</a></li>
<li><a href="../foundation/donate.html">Donate</a></li>
</ul>

<p class="copyright">
Portions of this content are &copy; 1998&#8211;2006 by individual mozilla.org
contributors; content available under a Creative Commons license | <a
href="http://www.mozilla.org/foundation/licensing/website-content.html">Details</a>.</p>
</div>
</div>
</body>
</html>