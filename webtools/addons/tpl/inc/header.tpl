<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html lang="en">

<head>
    <title>{if $title}{$title} :: {/if}Mozilla Addons :: Add Features to Mozilla Software</title>
    <meta http-equiv="content-type" content="text/html; charset=utf-8">
    <meta name="keywords" content="mozilla update, mozilla extensions, mozilla plugins, thunderbird themes, thunderbird extensions, firefox extensions, firefox themes">
    <link rel="stylesheet" type="text/css" href="{$config.webpath}/css/print.css" media="print">
    <link rel="stylesheet" type="text/css" href="{$config.webpath}/css/base/content.css" media="all">
    <link rel="stylesheet" type="text/css" href="{$config.webpath}/css/cavendish/content.css" title="Cavendish" media="all">
    <link rel="stylesheet" type="text/css" href="{$config.webpath}/css/base/template.css" media="screen">
    <link rel="stylesheet" type="text/css" href="{$config.webpath}/css/cavendish/template.css" title="Cavendish" media="screen">
    <link rel="home" title="Home" href="https://addons.mozilla.org/">
    <link rel="alternate" type="application/rss+xml" title="New Firefox Extensions Additions" href="{$config.webpath}/rss/?app=firefox&amp;type=E&amp;list=newest">
    <link rel="icon" href="/favicon.ico" type="image/png">
</head>

<body id="update-mozilla-org">
<div id="container">

<p class="skipLink"><a href="#firefox-feature" accesskey="2">Skip to main content</a></p>
<div id="mozilla-org"><a href="http://www.mozilla.org/">Visit Mozilla.org</a></div>

<div id="header">

<div id="key-title">
    <h1><a href="{$config.webpath}/" title="Return to home page" accesskey="1">Mozilla Update: Beta</a></h1>
    <ul>
        <li><a href="{$config.webpath}/login.php" title="Log in to MyUpdate">Login</a></li>
        <li><a href="{$config.webpath}/register.php" title="Register your MyUpdate account">Register</a></li>
        <li><a href="{$config.webpath}/faq.php" title="Frequently Asked Questions">FAQ</a></li>
        <li><a href="{$config.webpath}/search.php" title="Find an Addon">Search</a></li>
    </ul>
</div>
<!-- end key-title -->

<div id="key-menu" class="earth"> 
    <form id="search" method="get" action="{$config.webpath}/search.php" title="Search Mozilla Update">
    <div>
        <input type="text" id="q" name="q" accesskey="s" size="10" value="{$clean.q}">
        <input type="submit" id="submit" value="Search Addons">
    </div>
    </form>
    <div class="ie-clear-menu">&nbsp;</div>
</div>
<!-- end key-menu -->

</div>
<!-- end header -->

