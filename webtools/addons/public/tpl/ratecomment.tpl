<div id="mBody">

<h2>{$addon->Name} &raquo; Rate Comment</h2>
<p class="first">
<strong><a href="{$config.webpath}/{$app}/{$addon->ID}/">{$addon->Name} {$addon->Version}</a></strong>,
by <a href="{$config.webpath}/{$app}/{$addon->UserID}/author/">{$addon->UserName}</a>,
released on {$addon->VersionDateAdded|date_format}
</p>

<p><strong>Thanks for your input!</strong></p>
<p>You have successfully rated the following comment as <strong>{$clean.helpful}</strong>:</p>
<div id="comment-rate">
<h3>{$comment.CommentTitle}</h3>
<p>
{$comment.CommentNote}
</p>
</div>

<p>&laquo; <a href="./addon.php?id={$addon->ID}">Return to information about{$addon->Name}</a></p>

</div>
