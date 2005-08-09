<div class="tabs">
<ul>
{section name=tabs loop=$tabs}
{if $tabs[tabs].app eq $app}
<li><a href="./?app={$tabs[tabs].app|escape}" class="active-tab">{$tabs[tabs].app|escape}</a></li>
{else}
<li><a href="./?app={$tabs[tabs].app|escape}">{$tabs[tabs].app|escape}</a></li>
{/if}
{/section}
</ul>
</div>

<div id="mainContent" class="right">
<h2>Newest {$app} Addons</h2>
<ol class="popularlist">
{section name=new loop=$newest}
<li>
<a href="./addon.php?id={$newest[new].ID}">{$newest[new].Name} {$newest[new].Version}</a> ({$newest[new].DateAdded|date_format}) - {$newest[new].Description|escape} <a href="./addon.php?id={$newest[new].ID}">more...</a>
</li>
{/section}
</ol>
<p><strong><a href="./search.php?app={$app}&amp;sort=newest">More ...</a></strong></p>

<h2>Popular {$app} Extensions</h2>
<ol class="popularlist">
{section name=pe loop=$popularExtensions}
<li>
<a href="./addon.php?id={$popularExtensions[pe].ID}">{$popularExtensions[pe].name}</a> <span class="downloads">({$popularExtensions[pe].Rating} rating, {$popularExtensions[pe].dc} downloads)</span> - {$popularExtensions[pe].Description|escape} <a href="./addon.php?id={$popularExtensions[pe].ID}">more...</a>
</li>
{/section}
</ol>
<p><strong><a href="./search.php?app={$app}&amp;sort=popular&amp;type=E">More ...</a></strong></p>

<h2>Popular {$app} Themes</h2>
<ol class="popularlist">
{section name=pt loop=$popularThemes}
<li><a href="./addon.php?id={$popularThemes[pt].ID}">{$popularThemes[pt].name}</a> <span class="downloads">({$popularThemes[pt].Rating} rating, {$popularThemes[pt].dc} downloads)</span> - {$popularThemes[pt].Description|escape} <a href="./addon.php?id={$popularThemes[pt].ID}">more...</a>
</li>
{/section}
</ol>
<p><strong><a href="./search.php?app={$app}&amp;sort=popular&amp;type=T">More ...</a></strong></p>
</div>
<!-- end mainContent -->

<div id="side" class="right">
<h2>What is <kbd>addons.mozilla.org</kbd>?</h2>
<p><a href="./"><kbd>addons.mozilla.org</kbd></a> is the place to get updates and extras for
your <a href="http://www.mozilla.org/">Mozilla</a> products.</p>

<h2>What can I find here?</h2>
<dl>
<dt><a href="./search.php?app={$app}&amp;type=E">Extensions</a></dt>
<dd>Extensions are small add-ons that add new functionality to your
Mozilla program. They can add anything from a toolbar button to a
completely new feature.</dd>

<dt><a href="./search.php?app={$app}&amp;type=T">Themes</a></dt>
<dd>Themes allow you to change the way your Mozilla program looks. 
New graphics and colors.</dd>

<dt><a href="https://pfs.mozilla.org/plugins/">Plugins</a></dt>
<dd>Plugins are programs that allow websites to provide content to
you and have it appear in your browser. Examples of Plugins are Flash,
RealPlayer, and Java.</dd>
</dl>

<h2>How can I contribute?</h2>
<ul>
<li><a href="#">Help us review new Addons.</a></li>
<li><a href="#">Develop your own Addon.</a></li>
<li><a href="#">Report a problem with an Addon.</a></li>
<li><a href="#">Make suggestions or comments about this site.</a></li>
</ul>
</div>
<!-- end side -->
