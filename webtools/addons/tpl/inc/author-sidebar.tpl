
<ul id="nav">
<li><span>Addons by this author...</span>
    <ul>
    {section name=addons loop=$user->AddOns}
    <li><a href="./addon.php?id={$user->AddOns[addons].ID}" title="See other Addons in this category.">{$user->AddOns[addons].Name}</a></li>
    {/section}
    </ul>
</li>
</ul>

