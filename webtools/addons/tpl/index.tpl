
<div id="mainContent" class="right">
<h2>What is Mozilla Update?</h2>

<p class="first">Mozilla Update is the place to get updates and extras for
your <a href="http://www.mozilla.org/">Mozilla</a> products.</p>

<h2>What can I find here?</h2>
<dl>
<dt>Extensions</dt>
<dd>Extensions are small add-ons that add new functionality to your
Mozilla program. They can add anything from a toolbar button to a
completely new feature. Browse extensions for: 
<a href="./overview.php?app=firefox">Firefox</a>, 
<a href="./overview.php?app=thunderbird">Thunderbird</a>,
<a href="./overview.php?app=mozilla">Mozilla Suite</a>
</dd>

<dt>Themes</dt>
<dd>Themes allow you to change the way your Mozilla program looks. 
New graphics and colors. Browse themes for: 
<a href="./overview.php?app=firefox">Firefox</a>,
<a href="./overview.php?app=thunderbird">Thunderbird</a>,
<a href="./overview.php?app=mozilla">Mozilla Suite</a>
</dd>

<dt>Plugins</dt>
<dd>Plugins are programs that allow websites to provide content to
you and have it appear in your browser. Examples of Plugins are Flash,
RealPlayer, and Java. Browse plug-ins for: 
<a href="https://pfs.mozilla.org/plugins/">Mozilla Suite &amp; Firefox</a>
</dd>
</dl>
</div>
<!-- end mainContent -->

<div id="side" class="right">
<h2>Popular Extensions</h2>
<ol class="popularlist">
{section name=pe loop=$popularExtensions}
<li><a href="./addon.php?id={$popularExtensions[pe].id}">{$popularExtensions[pe].name}</a> <span class="downloads">({$popularExtensions[pe].dc} downloads)</span></li>
{/section}
</ol>

<h2>Popular Themes</h2>
<ol class="popularlist">
{section name=pt loop=$popularThemes}
<li><a href="./addon.php?id={$popularThemes[pt].id}">{$popularThemes[pt].name}</a> <span class="downloads">({$popularThemes[pt].dc} downloads)</span></li>
{/section}
</ol>

<h2>Newest Addons</h2>
<ol class="popularlist">
{section name=new loop=$newest}
<li><a href="./addon.php?id={$newest[new].ID}">{$newest[new].Name} {$newest[new].Version}</a> ({$newest[new].DateAdded|date_format})</li>
{/section}
</ol>
</div>
<!-- end side -->

