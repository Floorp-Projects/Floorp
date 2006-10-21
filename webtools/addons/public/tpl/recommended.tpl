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

<div class="search-container corner-box">
	<img src="{$config.webpath}/images/rustico/featured/firefox-featured-mglass.png" width="37" height="31" alt="" />
	<h3>Find more Add-ons:</h3>
	<form id="extensions-search" method="get" action="" title="Search Mozilla Add-ons">
	    <input id="q2" type="text" name="q" accesskey="s" class="keywords" />
	    <select name="type">
        	<option value="A">all Add-ons</option>
        	<option value="E">just Extensions</option>
			<option value="T">just Themes</option>
			</select>
	    <input type="submit" value="Search" />
	</form>
</div>
