{if $app eq "firefox"}
<h1>Recommended Add-ons</h1>

<p>With over a thousand add-ons available, there's something for everyone. To get you started, here's a list of some of our favorites.  Enjoy!</p>

{section name=re loop=$recommended step=1 start=0}
	<div class="addon-feature clearfix divider-bottom">
		<img src="{$config.webpath}{$recommended[re].previewuri}" alt="" class="addon-feature-image" />
		<div class="addon-feature-text corner-box">
			<h2><a href="{$config.webpath}/{$app}/{$recommended[re].id}/">{$recommended[re].name}</a> <span>by {$recommended[re].username}</span></h2>
			<p>{$recommended[re].body}</p>
			<p class="install-button"><a href="{$recommended[re].uri}" onclick="return install(event,'{$recommended[re].name} {$recommended[re].version}', '{$config.webpath}/images/default.png', '{$recommended[re].hash}');" title="Install {$recommended[re].name} {$recommended[re].version} (Right-Click to Download)"><span class="install-green-button"><span class="install-button-text">Install now ({$recommended[re].size}&nbsp;KB)</span></span></a></p>
      {if $recommended[re].id eq SUPER_MAGIC_YAHOO_ID}
      <p class="eula">By clicking Install, you agree to the <a href="http://del.icio.us/help/firefox/bookmarks/license">Software License</a> for this add-on.</p>
      {/if}			
		</div>
	</div>
{/section}

{include file="inc/search-box.tpl"}
{else}
<div id="mBody">
<h1>Getting Started with Extensions</h1>

<p>With over a thousand extensions available, there's something for everyone. To get you started, here's a list of some of our recommended extensions that make use of popular online services.</p>

{section name=re loop=$recommended step=1 start=0}
<div class="recommended">

<a href="{$config.webpath}/{$app}/{$recommended[re].id}/previews/"><img class="recommended-img" alt="" src="{$config.webpath}{$recommended[re].previewuri}"/></a>
<h2><a href="{$config.webpath}/{$app}/{$recommended[re].id}/">{$recommended[re].name}</a></h2>
<div class="recommended-download">
<h3><a href="{$recommended[re].uri}" onclick="return install(event,'{$recommended[re].name} {$recommended[re].version}', '{$config.webpath}/images/default.png', '{$recommended[re].hash}');" title="Install {$recommended[re].name} {$recommended[re].version} (Right-Click to Download)">Install Extension ({$recommended[re].size} KB)</a></h3>
</div>
<p>{$recommended[re].body|nl2br}</p>
</div>
{/section}
</div>

<br class="clear-both"/>
{/if}