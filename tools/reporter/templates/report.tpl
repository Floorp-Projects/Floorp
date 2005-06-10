{if $error != ''}<h3 class="rmo_error">Error: {$error}</h3>{else}
<table id="report_table">
	<tr>
		<th>Report ID:</th>
		<td>{$report_id}</td>
	</tr>
	<tr>
		<th>URL:</th>
		<td><a href="{$report_url}" rel="nofollow external" title="Visit the page">{$report_url|truncate:60}</a></td>
	</tr>
	<tr>
		<th>Host:</th>
		<td><a href="{$host_url}" title="see all reports for {$host_hostname}">Reports for {$host_hostname}</a></td>
	</tr>
	<tr>
		<th>Problem Type:</th>
		<td>{$report_problem_type}</td>
	</tr>
	<tr>
		<th>Behind Login:</th>
		<td>{$report_behind_login}</td>
	</tr>
	<tr>
		<th>Product:</th>
		<td>{$report_product}</td>
	</tr>
	<tr>
		<th>Gecko Version:</th>
		<td>{$report_gecko}</td>
	</tr>
	<tr>
		<th>Platform:</th>
		<td>{$report_platform}</td>
	</tr>
	<tr>
		<th>OS/CPU:</th>
		<td>{$report_oscpu}</td>
	</tr>
	<tr>
		<th>Language:</th>
		<td>{$report_language}</td>
	</tr>
	<tr>
		<th>User Agent:</th>
		<td>{$report_useragent}</td>
	</tr>
	<tr>
		<th>Build Config:</th>
		<td>{$report_buildconfig}</td>
	</tr>
{if $is_admin == true}
	<tr>
		<th>Email:</th>
		<td>{$report_email}</td>
	</tr>
	<tr>
		<th>IP Address:</th>
		<td><a href="http://ws.arin.net/cgi-bin/whois.pl?queryinput={$report_ip}" rel="external" target="_blank" title="Lookup IP: {$report_ip}">{$report_ip}</a></td>
	</tr>
{/if}
	<tr>
		<th>Description:</th>
		<td>{$report_description}</td>
	</tr>
</table>{/if}

