<h2><strong>{$addon->Name}</strong> &raquo; Overview</h2>

<script type="text/javascript" src="/js/auto.js"></script>

<p class="first">
<strong><a href="./addon.php?id={$addon->ID}">{$addon->Name} {$addon->Version}</a></strong>,
by <a href="./author.php?id={$addon->UserID}">{$addon->UserName}</a>,
released on {$addon->VersionDateAdded|date_format}
</p>

{if $addon->PreviewURI}
<p class="screenshot">
<a href="./previews.php?id={$addon->ID}" title="See more {$addon->Name} previews.">
<img src="{$config.webpath}{$addon->PreviewURI}" alt="{$addon->Name} screenshot">
</a>
<strong><a href="./previews.php?id={$addon->ID}" title="See more {$addon->Name} previews.">More Previews &raquo;</a></strong>
</p>
{/if}

<p>{$addon->Description}</p>

<p class="requires">
Requires: {$addon->AppName} 1.0 - 1.0+ <img src="{$config.webpath}/img/{$addon->AppName|lower}_icon.png" width="34" height="34" alt="{$addon->AppName}">
</p>

<div class="key-point install-box">
<div class="install">
<b><a href="{$addon->URI}" onclick="return install(event,'{$addon->AppName|escape} {$addon->Version|escape}', '{$config.webpath}/img/default.png');" TITLE="Install {$addon->AppName|escape} {$addon->Version|escape} (Right-Click to Download)">Install Now</a></b> ({$addon->Size|escape} KB File)</div></div>

<h3 id="user-comments">User Comments</h3>

<p><strong><a href="./addcomment.php?id={$addon->ID}">Add your own comment &#187;</a></strong></p>

<ul id="opinions">
{section name=comments loop=$addon->Comments max=10}
<li>
<h4>{$addon->Comments[comments].CommentTitle} ({$addon->Comments[comments].CommentVote} rating)</h4>
<p class="opinions-info">by {$addon->Comments[comments].CommentName}, {$addon->Comments[comments].CommentDate|date_format}</p>
<p class="opinions-text">{$addon->Comments[comments].CommentNote}</p>
<p class="opinions-rating">Was this comment helpful? <a href="./ratecomment.php?id={$addon->Comments[comments].CommentID}&amp;r=yes">Yes</a> &#124; <a href="./ratecomment.php?id={$addon->Comments[comments].CommentID}&amp;r=no">No</a></p>
</li>
{/section}
</ul>

<p><strong><a href="./comments.php?id={$addon->ID}">Read all comments &#187;</a></strong></p>

<h3>Addon Details</h3>
<ul>
<li>Categories: 
<ul>
{section name=cats loop=$addon->AddonCats}
<li><a href="./search.php?cat={$addon->AddonCats[cats].CatName}" title="See other Addons in this category.">{$addon->AddonCats[cats].CatName}</a></li>
{/section}
</ul>
</li>
<li>Last Updated: {$addon->DateUpdated|date_format}</li>
<li>Total Downloads: {$addon->TotalDownloads} &nbsp;&#8212;&nbsp; Downloads this Week: {$addon->downloadcount}</li>
<li>See <a href="./history.php?id={$addon->ID}">all previous releases</a> of this addon.</li>
{if $addon->UserWebsite}
<li>View the <a href="{$addon->Homepage}">Author's homepage</a> for this addon.</li>
{/if}
</ul>

