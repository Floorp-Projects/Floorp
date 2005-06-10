{if $error != ''}<h3 class="rmo_error">Error: {$error}</h3>{/if}
{if !$error}
<table id="query_table">
	<tr class="header">
		{section name=mysec loop=$column}
  		<th>{if $column[mysec].url != null}<a href="{$column[mysec].url}">{/if}{$column[mysec].title}{if $column[mysec].url != null}</a>{/if}</th>
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
<div class="navigation">{$navigation}</div>
{/if}
