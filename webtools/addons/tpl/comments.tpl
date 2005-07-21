<h2><strong>{$addon->Name}</strong> &raquo; Comments</h2>
<p class="first">
<strong><a href="./addon.php?id={$addon->ID}">{$addon->Name} {$addon->Version}</a></strong>,
by <a href="./author.php?id={$addon->UserID}">{$addon->UserName}</a>,
released on {$addon->VersionDateAdded|date_format}
</p>

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

