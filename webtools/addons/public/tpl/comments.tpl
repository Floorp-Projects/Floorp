<div class="rating" title="{$addon->Rating} out of 5">Rating: {$addon->Rating}</div>
<h2><strong>{$addon->Name}</strong> &raquo; Comments</h2>
<p class="first">
<strong><a href="{$config.webpath}/{$app}/{$addon->ID}/">{$addon->Name} {$addon->Version}</a></strong>,
by 
{foreach key=key item=item from=$addon->Authors}
    <a href="{$config.webpath}/{$app}/{$item.UserID|escape}/author/">{$item.UserName|escape}</a>,
{/foreach}
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
<option value="datenewest"{if $page.orderby eq "datenewest" or !$page.orderby} selected="selected"{/if}>Newest</option>
<option value="mosthelpful"{if $page.orderby eq "mosthelpful"} selected="selected"{/if}>Most Helpful</option>
<option value="ratinghigh"{if $page.orderby eq "ratinghigh"} selected="selected"{/if}>Highest Rated</option>
<option value="dateoldest"{if $page.orderby eq "dateoldest"} selected="selected"{/if}>Oldest</option>
<option value="ratinglow"{if $page.orderby eq "ratinglow"} selected="selected"{/if}>Lowest Rated</option>
<option value="leasthelpful"{if $page.orderby eq "leasthelpful"} selected="selected"{/if}>Least Helpful</option>
</select>
<input class="amo-submit" type="submit" value="Go"/></div>
</form>
</div>
<div class="pages">
<div class="prev">
{if $page.left}
<a href="{$config.webpath}/comments.php?app={$app}&amp;{$page.url}&amp;left={$page.previous}&amp;orderby={$page.orderby}">&laquo; Previous Page</a>
{else}
&laquo; Previous Page
{/if}
</div>

<div class="next">
{if $page.next}
<a href="{$config.webpath}/comments.php?app={$app}&amp;{$page.url}&amp;left={$page.next}&amp;orderby={$page.orderby}">Next Page &raquo;</a>
{else}
Next Page &raquo;
{/if}
</div>
</div>



<ul id="opinions">
{section name=comments loop=$addon->Comments}
<li>
<div class="opinions-vote">{$addon->Comments[comments].CommentVote}<span class="opinions-caption">out of 5</span></div>
<h4 class="opinions-title">{$addon->Comments[comments].CommentTitle|strip_tags}</h4>
<p class="opinions-info">by 
{if $addon->Comments[comments].CommentName}
{$addon->Comments[comments].CommentName|strip_tags}
{else}
{$addon->Comments[comments].UserName|strip_tags}
{/if}
, {$addon->Comments[comments].CommentDate|date_format}</p>
<p class="opinions-text">{$addon->Comments[comments].CommentNote|strip_tags|nl2br}</p>
<p class="opinions-helpful"><strong>{$addon->Comments[comments].helpful_yes}</strong> out of <strong>{$addon->Comments[comments].helpful_total}</strong> viewers found this comment helpful<br>
Was this comment helpful? <a href="{$config.webpath}/ratecomment.php?aid={$addon->ID}&amp;cid={$addon->Comments[comments].CommentID}&amp;r=yes">Yes</a> &#124; <a href="{$config.webpath}/ratecomment.php?aid={$addon->ID}&amp;cid={$addon->Comments[comments].CommentID}&amp;r=no">No</a></p>
</li>
{/section}
</ul>

<div class="pages">
<div class="prev">
{if $page.left}
<a href="{$config.webpath}/comments.php?app={$app}&amp;{$page.url}&amp;left={$page.previous}&amp;orderby={$page.orderby}">&laquo; Previous Page</a>
{else}
&laquo; Previous Page
{/if}
</div>

<div class="next">
{if $page.next}
<a href="{$config.webpath}/comments.php?app={$app}&amp;{$page.url}&amp;left={$page.next}&amp;orderby={$page.orderby}">Next Page &raquo;</a>
{else}
Next Page &raquo;
{/if}
</div>
</div>

{else}
<p>There are no comments yet for this addon.  <a href="{$config.webpath}/addcomment.php?aid={$addon->ID}&amp;app={$app}"></a></p>
{/if}

