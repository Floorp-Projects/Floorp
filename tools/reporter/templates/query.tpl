{if $error != ''}<h3 class="rmo_error">Error: {$error}</h3>{/if}
{if !$error}
<table id="query_table">
	<tr class="header">
		{section name=mysec loop=$column}
  		<th>{if $column[mysec].url != null}<a href="{$column[mysec].url}">{/if}{$column[mysec].text}{if $column[mysec].url != null}</a>{/if}</th>
		{/section}
	</tr>
	{section name=mysec2 loop=$row}
	<tr>
		{section name=col loop=$row[mysec2]}
			<td>{if $row[mysec2][col].url != null}<a href="{$base_url}/app{$row[mysec2][col].url}">{/if}{$row[mysec2][col].text|truncate:100}{if $row[mysec2][col].url != null}</a>{/if}</td>
		{/section}
	</tr>
	{/section}
</table>
<div class="navigation">
{strip}
{math equation="($count/$show)" assign="totalPages"}

{if $page > 1}
<a href="?{$continuityParams}&amp;page=1">&lt;</a>
{/if}



{if ($page+20) > $totalPages}
{math equation="$page-($totalPages-($page-20))" assign="start"}
{math equation="$totalPages" assign="max"}
{else}
{math equation="$page" assign="start"}
{math equation="$page+20" assign="max"}
{/if}

{section name=pagelisting start=$start loop=$max step=1}
&nbsp; <a href="?{$continuityParams}&amp;page={$smarty.section.pagelisting.index}">{$smarty.section.pagelisting.index}</a>
{/section}




{if $page+20 < $count/$show}
 &nbsp; <a href="?{$continuityParams}&amp;page={$totalPages}">&gt;</a>
{/if}
{/strip}
{*
<hr />
<pre>
Count:    {$count}
Page:     {$page}
Show:     {$show}
TotPage:  {$totalPages}
</pre>
*}
</div>
{/if}
