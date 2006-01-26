
<ul id="nav">
<li><span>{$addon->Name}</span>
    <ul>
    <li><a href="./addon.php?id={$addon->ID}">Overview</a></li>
    {if $addon->PreviewID}
    <li><a href="./previews.php?id={$addon->ID}">Previews &amp; Screenshots</a></li>
    {/if}
    <li><a href="./comments.php?id={$addon->ID}">Comments</a></li>
    <li><a href="./history.php?id={$addon->ID}">Version History</a></li>
    <li><a href="./addcomment.php?id={$addon->ID}">Add a Comment</a></li>
    </ul>
</li>
<li><span>Find similar Addons...</span>
    <ul>
    {section name=cats loop=$addon->AddonCats}
    <li><a href="./search.php?cat={$addon->AddonCats[cats].CategoryID}" title="See other Addons in this category.">{$addon->AddonCats[cats].CatName}</a></li>
    {/section}
    <li><a href="./search.php?app={$addon->AppName}">Other {$addon->AppName} Addons</a></li>
    </ul>
</li>
<li><span>More about the author...</span>
    <ul>
    <li><a href="./author.php?id={$addon->UserID}">{$addon->UserName}</a></li>
    </ul>
</li>
</ul>

