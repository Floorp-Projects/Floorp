{if $error != ''}
    <div class="error">
        <h3>Error</h3>
        <p>{$error}.  <a href="javascript:history.go(-1);">Back</a></p>
    </div>
{else}
{if $notice != ''}
    <div class="notice">
         <h3>Notice:</h3>
         <p>{$notice}</p>
    </div>
{/if}
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
    {if $count > $show}
        <div class="navigation">
            {strip}
            {if $page > 1}
                <a href="?{$continuity_params}&amp;page={$page-1}">&lt;</a>
            {/if}
            {/strip}
            {section name=pageLoop start=$start loop=$start+$amt step=$step}
                {strip}&nbsp; <a href="{$base_url}/app/query/?{$continuity_params}&amp;page={$smarty.section.pageLoop.index}"
                    {if $smarty.section.pageLoop.index == $page}class="currentPage"{/if}
                       >{$smarty.section.pageLoop.index}</a>
                {/strip}
            {/section}
            {strip}
                {if $page < $pages}
                     &nbsp; <a href="{$base_url}/app/query/?{$continuity_params}&amp;page={$page+1}">&gt;</a>
                {/if}
            {/strip}
            <hr />
            {*
            <pre>
            Count:    {$count}
            Page:     {$page}
            Show:     {$show}
            Start:    {$start}
            Max:      {$max}
            TotPage:  {$pages}
            </pre>
            *}
            <br />
            <p><small>Your query returned {$count} reports</small></p>
        </div>
    {/if}
{/if}