
<ul id="nav">
<li><span>{$addon->Name}</span>
    <ul>
    <li><a href="{$config.webpath}/{$app}/{$addon->ID}/">Overview</a></li>
    {if $addon->PreviewID}
    <li><a href="{$config.webpath}/{$app}/{$addon->ID}/previews/">Previews &amp; Screenshots</a></li>
    {/if}
    <li><a href="{$config.webpath}/{$app}/{$addon->ID}/comments/">Comments</a></li>
    <li><a href="{$config.webpath}/addcomment.php?aid={$addon->ID}&amp;app={$app}">Add a Comment</a></li>
    <li><a href="{$config.webpath}/{$app}/{$addon->UserID}/author/">About the Author</a></li>
    <li><a href="{$config.webpath}/{$app}/{$addon->ID}/history/">Version History</a></li>
    </ul>
</li>
<li><span>Find Similar Add-ons</span>
    <ul>
    {section name=cats loop=$addon->AddonCats}
    <li><a href="./search.php?cat={$addon->AddonCats[cats].CategoryID}" title="See other Addons in this category.">{$addon->AddonCats[cats].CatName}</a></li>
    {/section}
    <li><a href="./search.php?app={$addon->AppName}">Other {$addon->AppName} Addons</a></li>
    </ul>
</li>
</ul>

