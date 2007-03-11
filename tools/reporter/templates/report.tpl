{if $screenshot == true}
{literal}
<script type="text/javascript">
function show(aItem){
	if(aItem == 'data'){
		document.getElementById('report_screenshot').style.display = 'none';
		document.getElementById('report_data').style.display = '';
	} else {
		document.getElementById('report_screenshot').innerHTML="<img src=\"screenshot/?report_id={/literal}{$report_id}{literal}\" alt=\"Screenshot for {/literal}{$report_id}{literal}\" id=\"screenshot\" />";
		document.getElementById('report_screenshot').style.display = '';
		document.getElementById('report_data').style.display = 'none';
	}
}
</script>
{/literal}
{/if}

{literal}
<style type="text/css">
#report_tabs ul li {
	display: inline;
	border: 1px solid #ccc;
	background: 1px solid #eee;
	padding: 0.5em; 2em;
	margin: 0.5em; 4em;
}

#report_screenshot {
	margin: 0 auto 0 auto;
	text-align: center;
}
</style>
{/literal}

{if $error != ''}
<div id="error">
	<h3>Error</h3>
	<p>{$error}.  <a href="javascript:history.go(-1);">Back</a></p>
</div>
{else}
<div id="reporterReport">
	<div class="header">Report {$report_id}</div>
	<div id="report_data">
		<div>
			<div class="title">URL:</div>
			<div class="data"><a href="{$report_url}" rel="nofollow external" target="_blank" title="Visit Page">{$report_url|truncate:60}</a></div>
		</div>
		<div>
			<div class="title">Host:</div>
			<div class="data"><a href="{$host_url}" title="see all reports for {$host_hostname}">Reports for {$host_hostname}</a></div>
		</div>
		<div>
			<div class="title">Problem Type:</div>
			<div class="data">{$report_problem_type}</div>
		</div>
		<div>
		 	<div class="title">Behind Login:</div>
			<div class="data">{$report_behind_login}</div>
		</div>
		<div>
			<div class="title">Product:</div>
			<div class="data">{$report_product}</div>
		</div>
		<div>
		 	<div class="title">Gecko Version:</div>
			<div class="data">{$report_gecko}</div>
		</div>
		<div>
			<div class="title">Platform:</div>
			<div class="data">{$report_platform}</div>
		</div>
		<div>
			<div class="title">OS/CPU:</div>
			<div class="data">{$report_oscpu}</div>
		</div>
		<div>
			<div class="title">Language:</div>
			<div class="data">{$report_language}</div>
		</div>
		<div>
			<div class="title">Character Set:</div>
			<div class="data">{$report_charset}&nbsp;</div>
		</div>
		<div>
			<div class="title">User Agent:</div>
			<div class="data">{$report_useragent}</div>
		</div>
		<div>
			<div class="title">Build Config:</div>
			<div class="data">{$report_buildconfig}</div>
		</div>
		<div>
			<div class="title">Date Reported:</div>
			<div class="data">{$report_file_date}</div>
		</div>
		{if $is_admin == true}
		<div>
			<div class="title">Email:</div>
			<div class="data">{$report_email}&nbsp; {*this space at the end fixes some formatting issues with no text in this optional field *}</div>
		</div>
		<div>
			<div class="title">IP Address:</div>
			<div class="data"><a href="http://ws.arin.net/cgi-bin/whois.pl?queryinput={$report_ip}" rel="external" target="_blank" title="Lookup IP: {$report_ip}">{$report_ip}</a>&nbsp; {*this space at the end fixes some formatting issues with no text in this optional field *}</div>
		</div>
		{/if}
		<div>
			<div class="title">Description:</div>
			<div class="data">{$report_description}&nbsp; {*this space at the end fixes some formatting issues with no text in this optional field *}</div>
		</div>
	</div>
	{if $screenshot == true}
	<div id="report_screenshot" style="display:none;"></div>
	{/if}
	{if $is_admin == true}
	<div id="report_tabs">
		<ul>
			<li><a href="javascript:show('data');">Report Data</a></li>
			{if $screenshot == true}
			<li><a href="javascript:show('screenshot');">Screenshot</a></li>
			{/if}
		</ul>
	</div>
{/if}
</div>
<div id="reportNavigation">
	<p>
	{if $showReportNavigation == true}
		<strong>Report List ({$index+1} of {$total}):</strong> &nbsp;
		{strip}
		{if $first_report != 'disable'}
			<a href="{$base_url}/app/report/?report_id={$first_report}&amp;{$nav_continuity_params}" accesskey="f" title="First Report in List (Access Key 'F')">
		{/if}
		First
		{if $first_report != 'disable'}
			</a>
		{/if}
		{/strip}

	|

		{strip}
		{if $previous_report != 'disable'}
		<a href="{$base_url}/app/report/?report_id={$previous_report}&amp;{$nav_continuity_params}" accesskey="p" title="Previous Report in List (Access Key 'p')">
		{/if}
		Previous
		{if $previous_report != 'disable'}
		</a>
		{/if}
		{/strip}

	|

		{strip}
		{if $next_report != 'disable'}
		<a href="{$base_url}/app/report/?report_id={$next_report}&amp;{$nav_continuity_params}" accesskey="n" title="Next Report in List (Access Key 'N')">
		{/if}
		Next
		{if $next_report != 'disable'}
		</a>
		{/if}
		{/strip}

	|

		{strip}
		{if $last_report != 'disable'}
		<a href="{$base_url}/app/report/?report_id={$last_report}&amp;{$nav_continuity_params}" accesskey="l" title="Last Report in List (Access Key 'L')">
		{/if}
		Last
		{if $last_report != 'disable'}
		</a>
		{/if}
		{/strip}

	|

	{/if}
	<a href="{$base_url}/app/query/?{$nav_continuity_params}" accesskey="b" title="Back to the Query List (Access Key 'B')">Back To List</a>

	|
	<a href="{$base_url}/app/" accesskey="q" title="New Search (Access Key 'Q')">New Search</a>
	</p>
</div>
{/if}
