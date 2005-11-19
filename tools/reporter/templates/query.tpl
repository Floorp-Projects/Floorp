{if $error != ''}
    <div class="error">
        <h3>Error</h3>
        <p>{$error}.  <a href="javascript:history.go(-1);">Back</a></p>
    </div>
{else}
<table id="reporterQuery" class="fixedTable" cellspacing="0" cellpadding="0" style="width:100%">
	<tr class="header">
		{section name=mysec loop=$column}
  		<th>{if $column[mysec].url != null}<a href="{$column[mysec].url}">{/if}{$column[mysec].text}{if $column[mysec].url != null}</a>{/if}</th>
		{/section}
	</tr>
{if $method == 'html'}
	{section name=mysec2 loop=$row}
	<tr class="data{cycle values=', alt'}">
		{section name=col loop=$row[mysec2]}
			<td>
                            {strip}
                                {if $row[mysec2][col].url != null}
                                    <a href="{$base_url}/app{$row[mysec2][col].url}">
                                {/if}
                                {if $row[mysec2][col].col == 'report_url'}
                                    {$row[mysec2][col].text|truncate:50}
                                {else}
                                    {$row[mysec2][col].text|truncate:100}
                                {/if}
                                {if $row[mysec2][col].url != null}
                                    </a>
                                {/if}
                            {/strip}
                        </td>
		{/section}
	</tr>
	{/section}
{/if}
</table>
{/if}

{if $method == 'html'}

{if $count > 0}
<div class="navigation">
{strip}
    {math equation="($count/$show)" assign="totalPages"}

    <a href="?{$continuityParams}&amp;page=1">&lt;</a>

    {if ($page+20) > $totalPages}
        {math equation="$page-($totalPages-($page-20))" assign="start"}
        {math equation="$totalPages" assign="max"}
    {else}
        {math equation="$page" assign="start"}
        {math equation="$page+20" assign="max"}
    {/if}
{/strip}
{section name=pagelisting start=$start loop=$max step=1}
{strip}&nbsp; <a href="{$base_url}/app/query/?{$continuityParams}&amp;page={$smarty.section.pagelisting.index}"
       {if $smarty.section.pagelisting.index == $page}class="currentPage"{/if}
       >{$smarty.section.pagelisting.index}</a>
{/strip}
{/section}
{strip}
    {if $page+20 < $count/$show}
         &nbsp; <a href="{$base_url}/app/query/?{$continuityParams}&amp;page={$totalPages}">&gt;</a>
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
{/if}