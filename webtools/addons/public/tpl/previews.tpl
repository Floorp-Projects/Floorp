<div class="rating" title="{$addon->Rating} out of 5">Rating: {$addon->Rating}</div>
<h2><strong>{$addon->Name}</strong> &raquo; Previews &amp; Screenshots</h2>
<p class="first">
<strong><a href="{$config.webpath}/{$app}/{$addon->ID}/">{$addon->Name} {$addon->Version}</a></strong>,
by 
{foreach key=key item=item from=$addon->Authors}
    <a href="{$config.webpath}/{$app}/{$item.UserID|escape}/author/">{$item.UserName|escape}</a>,
{/foreach}
released on {$addon->VersionDateAdded|date_format}
</p>

{if count($addon->Previews) > 0 }
    {section name=preview loop=$addon->Previews}
    <h4>{$addon->Previews[preview].caption}</h4>
    <img src="{$config.webpath}{$addon->Previews[preview].PreviewURI}" alt="{$addon->Previews[preview].caption}" width="{$addon->Previews[preview].width}" height="{$addon->Previews[preview].height}"><br>

    {/section}
{else}
    <p>There are no previews or screenshots for this add-on.</p>
{/if}
