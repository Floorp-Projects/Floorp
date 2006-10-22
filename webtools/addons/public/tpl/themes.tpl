{if $app eq "firefox"}
<h1>Themes</h1>

<p>Themes are new clothes for your Firefox, another great way to personalize it to your tastes.  A theme can change only a few colors, or it can change every piece of Firefox's appearance.</p>

<div class='corner-box compact-list'>
<h3>Browse Themes by Category</h3>
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
<h3>Popular Themes</h3>
{section name=pt loop=$popularThemes step=1 start=0 max=5}
<a href="{$config.webpath}/{$app}/{$popularThemes[pt].ID}/">{$popularThemes[pt].name|truncate:30:"&hellip;"}</a><br>
{/section}
<br>
<a href="{$config.webpath}/search.php?app={$app}&amp;appfilter={$app}&amp;type=T&amp;sort=downloads">More&hellip;</a>
</div>

<div class='compact-list corner-box'>
<h3>New and Updated</h3>
{section name=nt loop=$newestThemes step=1 start=0 max=5}
<a href="{$config.webpath}/{$app}/{$newestThemes[nt].ID}/">{$newestThemes[nt].name|truncate:30:"&hellip;"}</a> <span>({$newestThemes[nt].dateupdated|date_format})</span><br>
{/section}
<br>
<a href="{$config.webpath}/search.php?app={$app}&amp;appfilter={$app}&amp;type=T&amp;sort=newest">More&hellip;</a>
</div>

{include file="inc/search-box.tpl" selected="T"}
{else}
<h1>Themes</h1>

<p>Themes are skins for Mozilla applications.  They allow you to change the look and feel of the user interface and personalize it to your tastes. A theme can simply change colors, or it can change every piece of its appearance.</p>

<h2><a href="{$config.webpath}/rss/{$app}/themes/updated/"><img class="rss" src="{$config.webpath}/images/rss.png" alt=""/></a><a href="{$config.webpath}/search.php?app={$app}&amp;appfilter={$app}&amp;type=T&amp;sort=newest">Recently Updated</a></h2>
<ul>
{section name=nt loop=$newestThemes step=1 start=0}
<li><a href="{$config.webpath}/{$app}/{$newestThemes[nt].ID}/">{$newestThemes[nt].name} {$newestThemes[nt].version}</a> ({$newestThemes[nt].dateupdated|date_format})</li>
{/section}
</ul>

<h2><a href="{$config.webpath}/rss/{$app}/themes/popular/"><img class="rss" src="{$config.webpath}/images/rss.png" alt=""/></a><a href="{$config.webpath}/search.php?app={$app}&amp;appfilter={$app}&amp;type=T&amp;sort=downloads">Top Themes</a></h2>
<ul>
{section name=pt loop=$popularThemes step=1 start=0}
<li><a href="{$config.webpath}/{$app}/{$popularThemes[pt].ID}/">{$popularThemes[pt].name}</a> ({$popularThemes[pt].dc} downloads)</li>
{/section}
</ul>
{/if}