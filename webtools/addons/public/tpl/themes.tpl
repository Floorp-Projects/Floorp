<h1>Themes</h1>

<p>Themes are skins for Mozilla applications.  They allow you to change the look and feel of the user interface and personalize it to your tastes. A theme can simply change colors, or it can change every piece of its appearance.</p>

<h2><a href="{$config.webpath}/rss/{$app}/themes/updated/"><img class="rss" src="{$config.webpath}/images/rss.png"/>Recently Updated</a></h2>
<ul>
{section name=nt loop=$newestThemes step=1 start=0}
<li><a href="{$config.webpath}/{$app}/{$newestThemes[nt].ID}/">{$newestThemes[nt].name} {$newestThemes[nt].version}</a> ({$newestThemes[nt].dateupdated|date_format})</li>
{/section}
</ul>

<h2><a href="{$config.webpath}/rss/{$app}/themes/popular/"><img class="rss" src="{$config.webpath}/images/rss.png"/>Top Themes</h2>
<ul>
{section name=pt loop=$popularThemes step=1 start=0}
<li><a href="{$config.webpath}/{$app}/{$popularThemes[pt].ID}/">{$popularThemes[pt].name}</a> ({$popularThemes[pt].dc} downloads)</li>
{/section}
</ul>
