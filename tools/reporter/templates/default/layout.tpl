<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html lang="en">

<head>
 <title>{$title}</title>
 <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">


 <link rel="stylesheet" type="text/css" href="{$base_url}/app/styles/style.css" media="all">
 <link rel="icon" href="images/mozilla-16.png" type="image/png">
 <link rel="home" title="Home" href="http://www.mozilla.org/">
</head>
<body id="www-mozilla-org">
<div id="container">
    <p class="skipLink"><a href="#content" accesskey="2">Skip to main content</a></p>
    <div id="mozilla-org"><a href="http://www.mozilla.org/">Visit Mozilla.org</a></div>
    <div id="header">
        <div id="logo">
            {* for validation reasons two <a/> are used *}
            <a href="{$base_url}/app">
                <img src="{$base_url}/app/img/reporter.gif" alt="Reporter" />
            </a>
            <h1><a href="{$base_url}/app">Mozilla Reporter</a></h1>
        </div>
        <div id="navbox">
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
        </div>
        <br style="clear: both;" />
    </div>
    <!--
    reporter.mozilla.org by:
      Robert Accettura <http://robert.accettura.com>
    -->
    <div id="content">
    {$content}
    </div>
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
       <p>Copyright &copy; 1998-2005 The Mozilla Organization</p>
    </div>
    <!-- closes #footer-->
</div>
<!-- closes #container -->

</body>
</html>
