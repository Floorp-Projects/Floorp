<h2>{$addon->Name} &raquo; Rate Comment</h2>
<p class="first">
<strong><a href="{$config.webpath}/{$app}/{$addon->ID}/">{$addon->Name} {$addon->Version}</a></strong>,
by 
{foreach key=key item=item from=$addon->Authors}
    <a href="{$config.webpath}/{$app}/{$item.UserID|escape}/author/">{$item.UserName|escape}</a>,
{/foreach}
released on {$addon->VersionDateAdded|date_format}
</p>

<p><strong>Thanks for your input!</strong> Your rating will appear shortly.</p>
<p>You have successfully rated the following comment as <strong>{$clean.helpful}</strong>:</p>
<div id="comment-rate">
<h3>{$comment.CommentTitle}</h3>
<p>
{$comment.CommentNote}
</p>
</div>

<p>&laquo; <a href="./addon.php?id={$addon->ID}">Return to information about {$addon->Name}</a></p>
