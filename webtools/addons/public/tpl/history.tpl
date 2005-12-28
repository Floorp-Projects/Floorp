<h2><strong>{$addon->Name}</strong> &raquo; Version History</h2>
<p class="first">
<strong><a href="./addon.php?id={$addon->ID}">{$addon->Name} {$addon->Version}</a></strong>,
by <a href="./author.php?id={$addon->UserID}">{$addon->UserName}</a>,
released on {$addon->VersionDateAdded|date_format}
</p>

{section name=version loop=$addon->History}
<div>
<h3><a href="addon.php?id={$addon->ID}&amp;vid=$vid">{$addon->Name} {$addon->History[version].Version}</a></h3>
Released on {$addon->History[version].VerDateAdded|date_format:"%B %d, %Y"}<br>
{if $addon->History[version].Notes}
    {$addon->History[version].Notes|nl2br}<br><br>
{/if}

    <div style="height: 34px">
        <div class="iconbar"><img src="{$config.webpath}/images/download.png" height="34" width="34" title="Install {$addon->History[version].Name} (Right-Click to Download)" alt="">Install</a><br><span class="filesize">Size: {$addon->History[version].Size|escape} kb</span></div>
        <div class="iconbar"><img src="{$config.webpath}/images/{$addon->History[version].AppName|lower}_icon.png" width="34" height="34" alt="{$addon->History[version].AppName}"> For {$addon->History[version].AppName}:<BR>&nbsp;&nbsp;{$addon->History[version].MinAppVer} - {$addon->History[version].MaxAppVer}</div>
    </div>
</div>
<hr>
{/section}
