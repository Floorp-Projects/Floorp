{if $app eq "firefox"}
<h1>Extensions</h1>
<p>Extensions are small add-ons that add new functionality to
Firefox, from a simple toolbar button to a completely new feature.
They allow you to customize Firefox to fit your own needs and
preferences, while letting us keep Firefox itself light and lean.</p>

<div class='corner-box compact-list'>
<h3>Browse Extensions by Category</h3>
<table cellpadding="5">
{foreach name=cats key=id item=name from=$cats}
{if $smarty.foreach.cats.index % 4 eq 0}
<tr>
{/if}
<td><a href="{$config.webpath}/search.php?cat={$id}&amp;app={$app}&amp;appfilter={$app}&amp;type={$type}">{$name}</a></td>
{if $smarty.foreach.cats.index % 4 eq 3}
</tr>
{/if}
{/foreach}
</table>
</div>

<div id='top-extensions' class='compact-list corner-box'>
<h3>Popular Extensions</h3>
{section name=pe loop=$popularExtensions step=1 start=0 max=5}
<a href="{$config.webpath}/{$app}/{$popularExtensions[pe].ID}/">{$popularExtensions[pe].name|truncate:30:"&hellip;"}</a><br>
{/section}
<br>
<a href="{$config.webpath}/search.php?app={$app}&amp;appfilter={$app}&amp;type=E&amp;sort=downloads">More&hellip;</a>
</div>

<div class='compact-list corner-box'>
<h3>New and Updated</h3>
{section name=ne loop=$newestExtensions step=1 start=0 max=5}
<a href="{$config.webpath}/{$app}/{$newestExtensions[ne].ID}/">{$newestExtensions[ne].name|truncate:30:"&hellip;"}</a> <span>({$newestExtensions[ne].dateupdated|date_format})</span><br>
{/section}
<br>
<a href="{$config.webpath}/search.php?app={$app}&amp;appfilter={$app}&amp;type=E&amp;sort=newest">More&hellip;</a>
</div>

{include file="inc/search-box.tpl" selected="E"}
{else}
<h1>Extensions</h1>

<p>Extensions are small add-ons that add new functionality to
Mozilla applications. They can add anything from a toolbar
button to a completely new feature. They allow the application to be customized
to fit the personal needs of each user if they need additional features,
while minimizing the size of the application itself.</p>

<h2><a href="{$config.webpath}/rss/{$app}/extensions/updated/"><img class="rss" src="{$config.webpath}/images/rss.png" alt=""/></a><a href="{$config.webpath}/search.php?app={$app}&amp;appfilter={$app}&amp;type=E&amp;sort=newest">Recently Updated</a></h2>
<ul>
{section name=ne loop=$newestExtensions step=1 start=0}
<li><a href="{$config.webpath}/{$app}/{$newestExtensions[ne].ID}/">{$newestExtensions[ne].name} {$newestExtensions[ne].version}</a> ({$newestExtensions[ne].dateupdated|date_format})</li>
{/section}
</ul>

<h2><a href="{$config.webpath}/rss/{$app}/extensions/popular/"><img class="rss" src="{$config.webpath}/images/rss.png" alt=""/></a><a href="{$config.webpath}/search.php?app={$app}&amp;appfilter={$app}&amp;type=E&amp;sort=downloads">Top Extensions</a></h2>
<ul>
{section name=pe loop=$popularExtensions step=1 start=0}
<li><a href="{$config.webpath}/{$app}/{$popularExtensions[pe].ID}/">{$popularExtensions[pe].name}</a> ({$popularExtensions[pe].dc} downloads)</li>
{/section}
</ul>
{/if}