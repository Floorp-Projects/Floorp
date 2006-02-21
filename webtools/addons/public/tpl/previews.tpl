<h2><strong>{$addon->Name}</strong> &raquo; Previews &amp; Screenshots</h2>
<p class="first">
<strong><a href="{$config.webpath}/{$app}/{$addon->ID}/">{$addon->Name} {$addon->Version}</a></strong>,
by <a href="{$config.webpath}/{$app}/{$addon->UserID}/author/">{$addon->UserName}</a>,
released on {$addon->VersionDateAdded|date_format}
</p>

{section name=preview loop=$addon->Previews}
<h4>{$addon->Previews[preview].caption}</h4>
<img src="{$config.webpath}{$addon->Previews[preview].PreviewURI}" alt="{$addon->Previews[preview].caption}" width="{$addon->Previews[preview].width}" height="{$addon->Previews[preview].height}"><br>

{/section}
