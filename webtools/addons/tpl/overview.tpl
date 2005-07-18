
<div class="tabs">
<ul>
{section name=tabs loop=$tabs}
{if $tabs[tabs].app eq $app}
<li><a href="./overview.php?app={$tabs[tabs].app|escape}" class="active-tab">{$tabs[tabs].app|escape}</a></li>
{else}
<li><a href="./overview.php?app={$tabs[tabs].app|escape}">{$tabs[tabs].app|escape}</a></li>
{/if}
{/section}
</ul>
</div>

<h2>Newest {$app} Addons</h2>
<ol class="popularlist">
{section name=new loop=$newest}
<li>
<a href="./addon.php?id={$newest[new].ID}">{$newest[new].Name} {$newest[new].Version}</a> ({$newest[new].DateAdded|date_format}) - {$newest[new].Description} ...
</li>
{/section}
</ol>
<p><strong><a href="./search.php?app={$app}&amp;cat=Newest">More ...</a></strong></p>

<h2>Popular {$app} Extensions</h2>
<ol class="popularlist">
{section name=pe loop=$popularExtensions}
<li>
<a href="./addon.php?id={$popularExtensions[pe].id}">{$popularExtensions[pe].name}</a> <span class="downloads">({$popularExtensions[pe].Rating} rating, {$popularExtensions[pe].dc} downloads)</span> - {$popularExtensions[pe].Description} ...
</li>
{/section}
</ol>
<p><strong><a href="./search.php?app={$app}&amp;cat=Popular&amp;type=E">More ...</a></strong></p>

<h2>Popular {$app} Themes</h2>
<ol class="popularlist">
{section name=pt loop=$popularThemes}
<li><a href="./addon.php?id={$popularThemes[pt].id}">{$popularThemes[pt].name}</a> <span class="downloads">({$popularThemes[pt].Rating} rating, {$popularThemes[pt].dc} downloads)</span></li>
{/section}
</ol>
<p><strong><a href="./search.php?app={$app}&amp;cat=Popular&amp;type=T">More ...</a></strong></p>

