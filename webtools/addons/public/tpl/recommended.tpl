{if $app eq "firefox"}
<div id="mBody">
<h1>Recommended Add-ons</h1>

<p>With over a thousand add-ons available, there's something for everyone. To get you started, here's a list of some of our favorites.  Enjoy!</p>

{section name=re loop=$recommended step=1 start=0}
	<div class="addon-feature clearfix">
		<img src="{$config.webpath}{$recommended[re].previewuri}" alt="" class="addon-feature-image" />
		<div class="addon-feature-text corner-box">
			<h4><a href="{$config.webpath}/{$app}/{$recommended[re].id}">{$recommended[re].name}</a> <span>by {$recommended[re].username}</span></h4>
			<p>{$recommended[re].body}</p>
			<p class="install-button"><a href="{$recommended[re].uri}" onclick="return install(event,'{$recommended[re].name} {$recommended[re].version}', '{$config.webpath}/images/default.png', '{$recommended[re].hash}');" title="Install {$recommended[re].name} {$recommended[re].version} (Right-Click to Download)"><span>Install now ({$recommended[re].size} KB)</span></a></p>
		</div>
	</div>
{/section}

{include file="inc/search-box.tpl"}
{else}
<div id="mBody">
<h1>Getting Started with Extensions</h1>

<p>With over a thousand extensions available, there's something for everyone. To get you started, here's a list of some of our recommended extensions that make use of popular online services.</p>

{section name=re loop=$recommendedExtensions step=1 start=0}
<div class="recommended">

<a href="{$config.webpath}/{$app}/{$recommendedExtensions[re].id}/previews/"><img class="recommended-img" alt="" src="{$config.webpath}{$recommendedExtensions[re].previewuri}"/></a>
<h2><a href="{$config.webpath}/{$app}/{$recommendedExtensions[re].id}/">{$recommendedExtensions[re].name}</a></h2>
<div class="recommended-download">
<h3><a href="{$recommendedExtensions[re].uri}" onclick="return install(event,'{$recommendedExtensions[re].name} {$recommendedExtensions[re].version}', '{$config.webpath}/images/default.png', '{$recommendedExtensions[re].hash}');" title="Install {$recommendedExtensions[re].name} {$recommendedExtensions[re].version} (Right-Click to Download)">Install Extension ({$recommendedExtensions[re].size} KB)</a></h3>
</div>
<p>{$recommendedExtensions[re].body|nl2br}</p>
</div>
{/section}
</div>

<br class="clear-both"/>
{/if}