
<h2><strong>{$addon->Name}</strong> &raquo; Comments</h2>
<p class="first">
<strong><a href="{$config.webpath}/{$app}/{$addon->ID}/">{$addon->Name} {$addon->Version}</a></strong>,
by <a href="{$config.webpath}/{$app}/{$addon->UserID}/author/">{$addon->UserName}</a>,
released on {$addon->VersionDateAdded|date_format}
</p>

{if $addon->Comments}
<p class="first">Showing comments <b>{$page.leftDisplay}-{$page.right}</b> out of <b>{$page.resultCount}</b>.  </p>

<div id="comments-sort">
<form action="{$config.webpath}/comments.php" method="get">
<div>
<input type="hidden" name="app" value="{$app}"/>
<input type="hidden" name="id" value="{$addon->ID}"/>

<label>Sort by</label>
<select id="orderby" name="orderby">
<option value="mosthelpful"{if $orderby eq "mosthelpful" or !$orderby} selected="selected"{/if}>Most Helpful</option>
<option value="ratinghigh"{if $orderby eq "ratinghigh"} selected="selected"{/if}>Highest Rated</option>
<option value="datenewest"{if $orderby eq "datenewest"} selected="selected"{/if}>Newest</option>
<option value="dateoldest"{if $orderby eq "dateoldest"} selected="selected"{/if}>Oldest</option>
<option value="ratinglow"{if $orderby eq "ratinglow"} selected="selected"{/if}>Lowest Rated</option>
<option value="leasthelpful"{if $orderby eq "leasthelpful"} selected="selected"{/if}>Least Helpful</option>
</select>
<input class="amo-submit" type="submit" value="Go"/></div>
</form>
</div>
<div class="pages">
<div class="prev">
{if $page.left}
<a href="{$config.webpath}/comments.php?app={$app}&amp;{$page.url}&amp;left={$page.previous}">&laquo; Previous Page</a>
{else}
&laquo; Previous Page
{/if}
</div>

<div class="next">
{if $page.next}
<a href="{$config.webpath}/comments.php?app={$app}&amp;{$page.url}&amp;left={$page.next}">Next Page &raquo;</a>
{else}
Next Page &raquo;
{/if}
</div>
</div>



<ul id="opinions">
{section name=comments loop=$addon->Comments}
<li>
<div class="opinions-vote">{$addon->Comments[comments].CommentVote}<span class="opinions-caption">out of 5</span></div>
<h4 class="opinions-title">{$addon->Comments[comments].CommentTitle}</h4>
<p class="opinions-info">by {$addon->Comments[comments].CommentName}, {$addon->Comments[comments].CommentDate|date_format}</p>
<p class="opinions-text">{$addon->Comments[comments].CommentNote}</p>
<p class="opinions-helpful"><strong>{$addon->Comments[comments].helpful_yes}</strong> out of <strong>{$addon->Comments[comments].helpful_total}</strong> viewers found this comment helpful<br>
Was this comment helpful? <a href="{$config.webpath}/ratecomment.php?aid={$addon->ID}&amp;cid={$addon->Comments[comments].CommentID}&amp;r=yes">Yes</a> &#124; <a href="{$config.webpath}/ratecomment.php?aid={$addon->ID}&amp;cid={$addon->Comments[comments].CommentID}&amp;r=no">No</a></p>
</li>
{/section}
</ul>
{else}
<p>There are no comments yet for this addon.  <a href="{$config.webpath}/addcomment.php?aid={$addon->ID}&amp;app={$app}"></a></p>
{/if}

