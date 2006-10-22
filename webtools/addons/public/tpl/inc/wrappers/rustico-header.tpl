{if $smarty.get.app eq "thunderbird"}
    {assign var="app" value="thunderbird"}
{elseif $smarty.get.app eq "mozilla"}
    {assign var="app" value="mozilla"}
{elseif $smarty.get.app eq "seamonkey"}
    {assign var="app" value="mozilla"}
{else}
    {assign var="app" value="firefox"}
{/if}
{if $app eq "firefox"}
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en-US" lang="en-US" dir="ltr">

<head>
	<meta http-equiv="Content-Type" content="text/html; charset=UTF-8"/>
	<title>{$app|capitalize} Add-ons | Mozilla Corporation</title>
	<link rel="stylesheet" type="text/css" href="/css/rustico/addons-rustico.css" media="screen">
  <script src="{$config.webpath}/js/install.js" type="text/javascript"></script>
</head>

<body id="mozilla-com">

	<div id="header">
		<div>
			<h1><img src="/images/rustico/header/moz-com-logo.png" height="38" width="89" alt="Mozilla Corporation" /></h1>
			<ul>
				<li id="menu-products"><a href="http://www.mozilla.com/products/">Products</a></li>
				<li id="menu-extensions"><a href="{$config.webpath}/firefox/">Add-ons</a></li>
				<li id="menu-support"><a href="http://www.mozilla.com/support/">Support</a></li>
				<li id="menu-developers"><a href="http://developer.mozilla.org/">Developers</a></li>
				<li id="menu-aboutus"><a href="http://www.mozilla.com/about/">About</a></li>
			</ul>
		</div>
	</div>
	
	<div id="page-title">
		<div>
			<h2><a href="firefox/"><img src="{$config.webpath}/images/rustico/common/addons-title.png" width="288" height="61" alt="" /></a></h2>
		</div>
	</div>

	<div id="container">
		
		<div id="menu-box">
			<ul>
{if $currentTab eq "home"}
        <li><span>Home</span></li>
{else}
        <li><a href="{$config.webpath}/">Home</a></li>
{/if}
{if $currentTab eq "recommended"}
        <li><span>Recommended Add-ons</span></li>
{else}
        <li><a href="{$config.webpath}/{$app}/recommended/">Recommended Add-ons</a></li>
{/if}
{if $currentTab eq "extensions"}
				<li><span>Extensions</span></li>
{else}
  			<li><a href="{$config.webpath}/{$app}/extensions/">Extensions</a></li>
{/if}
{if $currentTab eq "themes"}
        <li><span>Themes</span></li>
{else}
				<li><a href="{$config.webpath}/firefox/themes/">Themes</a></li>
{/if}
        <li><a href="{$config.webpath}/firefox/search-engines/">Search Engines</a></li>
				<li><a href="{$config.webpath}/firefox/plugins/">Plugins</a></li>
				<li><a href="http://developer.mozilla.org/en/docs/Extensions">Build Your Own</a></li>
			</ul>
		</div>

		<div id="mainContent">
{else}
{include file="inc/wrappers/default-header.tpl"}
{/if}