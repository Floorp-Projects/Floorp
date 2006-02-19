<h2><strong>{$addon->Name}</strong> &raquo; Version History</h2>
<p class="first">
<strong><a href="{$config.webpath}/{$app}/{$addon->ID}/">{$addon->Name} {$addon->Version}</a></strong>,
by <a href="{$config.webpath}/{$app}/{$addon->UserID}/author/">{$addon->UserName}</a>,
released on {$addon->VersionDateAdded|date_format}
</p>

<h3>Be Careful With Old Versions</h3>
<p>These versions are displayed for reference and testing purposes.  You should always use the latest version of an addon. </p>

<h3>Version History with Changelogs</h3>
<dl>
{section name=version loop=$addon->History}
<div>
<dt><a href="{$addon->History[version].URI}">{$addon->Name} {$addon->History[version].Version}</a> ({$addon->History[version].VerDateAdded|date_format:"%B %d, %Y"})</dt>
<dd>
{if $addon->History[version].Notes}
    {$addon->History[version].Notes|nl2br}<br><br>
{/if}
</dd>
</div>
{/section}
</dl>
