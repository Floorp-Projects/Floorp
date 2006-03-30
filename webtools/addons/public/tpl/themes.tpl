<h1>Themes</h1>

<p>Themes are skins for Mozilla applications.  They allow you to change the look and feel of the user interface and personalize it to your tastes. A theme can simply change colors, or it can change every piece of its appearance.</p>

<h2>Recently Updated</h2>
<ul>
{section name=nt loop=$newestThemes step=1 start=0}
<li><a href="{$config.webpath}/{$app}/{$newestThemes[nt].ID}/">{$newestThemes[nt].name} {$newestThemes[nt].version}</a> ({$newestThemes[nt].dateupdated|date_format})</li>
{/section}
</ul>

<h2>Top Themes</h2>
<ul>
{section name=pt loop=$popularThemes step=1 start=0}
<li><a href="{$config.webpath}/{$app}/{$popularThemes[pt].ID}/">{$popularThemes[pt].name}</a> ({$popularThemes[pt].dc} downloads)</li>
{/section}
</ul>
