<div id="mBody">
<h1>Recommended Add-ons</h1>

<p>With over a thousand add-ons available, there's something for everyone. To get you started, here's a list of some of our favorites.  Enjoy!</p>

{section name=re loop=$recommended step=1 start=0}
	<div class="addon-feature clearfix">
		<img src="{$config.webpath}{$recommended[re].previewuri}" alt="" class="addon-feature-image" />
		<div class="addon-feature-text corner-box">
			<h4>{$recommended[re].name} <span>by {$recommended[re].username}</span></h4>
			<p>{$recommended[re].body}</p>
			<p class="install-button"><a href="{$recommended[re].uri}" onclick="return install(event,'{$recommended[re].name} {$recommended[re].version}', '{$config.webpath}/images/default.png', '{$recommended[re].hash}');" title="Install {$recommended[re].name} {$recommended[re].version} (Right-Click to Download)"><span>Install now ({$recommended[re].size} KB)</span></a></p>
		</div>
	</div>
{/section}

{include file="inc/search-box.tpl"}
