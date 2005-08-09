
<h2><strong>{$addon->Name}</strong> &raquo; Comments</h2>
<p class="first">
<strong><a href="./addon.php?id={$addon->ID}">{$addon->Name} {$addon->Version}</a></strong>,
by <a href="./author.php?id={$addon->UserID}">{$addon->UserName}</a>,
released on {$addon->VersionDateAdded|date_format}
</p>

<ul id="opinions">
{section name=comments loop=$addon->Comments max=10}
<li>
<div class="opinions-vote">{$addon->Comments[comments].CommentVote}<span class="opinions-caption">out of 10</span></div>
<h4 class="opinions-title">{$addon->Comments[comments].CommentTitle}</h4>
<p class="opinions-info">by {$addon->Comments[comments].CommentName}, {$addon->Comments[comments].CommentDate|date_format}</p>
<p class="opinions-text">{$addon->Comments[comments].CommentNote}</p>
<p class="opinions-helpful"><strong>{$addon->Comments[comments].helpful_yes}</strong> out of <strong>{$addon->Comments[comments].helpful_total}</strong> viewers found this comment helpful<br>
Was this comment helpful? <a href="./ratecomment.php?aid={$addon->ID}&amp;cid={$addon->Comments[comments].CommentID}&amp;r=yes">Yes</a> &#124; <a href="./ratecomment.php?aid={$addon->ID}&amp;cid={$addon->Comments[comments].CommentID}&amp;r=no">No</a></p>
</li>
{/section}
</ul>

