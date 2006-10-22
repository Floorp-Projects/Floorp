<div class="search-container corner-box">
	<img src="{$config.webpath}/images/rustico/featured/firefox-featured-mglass.png" width="37" height="31" alt="" />
	<h3>Find more Add-ons:</h3>
	<form id="extensions-search" method="get" action="{$config.webpath}/search.php" title="Search Mozilla Add-ons">
	    <input id="q2" type="text" name="q" accesskey="s" class="keywords" />
	    <select name="type">
        	<option value="A">all Add-ons</option>
        	<option value="E" {if $selected eq "E"}selected{/if}>just Extensions</option>
			    <option value="T" {if $selected eq "T"}selected{/if}>just Themes</option>
			</select>
      <input type="hidden" name="app" value="{$app}"/>
	    <input type="submit" value="Search" />
	</form>
</div>