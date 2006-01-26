<h1>Themes</h1>

<p>Themes are skins for Mozilla applications.  They allow you to change the look and feel of the user interface and personalize it to your tastes. A theme can simply change colors, or it can change every piece of its appearance.</p>

<h2>Fresh from the Oven</h2>
<ul>
{section name=nt loop=$newestThemes step=1 start=0}
<li><a href="./addon.php?id={$newestThemes[nt].ID}">{$newestThemes[nt].name}</a> ({$newestThemes[nt].dateupdated|date_format})</li>
{/section}
</ul>

<h2>Hot this Week</h2>
<ul>
{section name=pt loop=$popularThemes step=1 start=0}
<li><a href="./addon.php?id={$popularThemes[pt].ID}">{$popularThemes[pt].name}</a> ({$popularThemes[pt].dc} downloads)</li>
{/section}
</ul>
