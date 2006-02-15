<div id="reporterReport">
	<div class="header">Statistics</div>
	<div>
		<div class="title">Unique Users:</div>
		<div class="data">{$users_quant}</div>
	</div>
	<div>
		<div class="title">Total Reports:</div>
		<div class="data">{$reports_quant}</div>
	</div>
	<div>
		<div class="title">Reports/User:</div>
		<div class="data">{$avgRepPerUsr}</div>
	</div>
	<div>
		<div class="title">Hostnames:</div>
		<div class="data">{$hosts_quant}</div>
	</div>
	<div>
		<div class="title">Reports (24hr):</div>
		<div class="data">{$reports24}</div>
	</div>
	<div>
		<div class="title">Reports (7d):</div>
		<div class="data">{$last7days}</div>
	</div>
	<div>
		<div class="title">By Platform:</div>
		<div class="data">
		<table>
    		{section name=sys loop=$platform}
			<tr>
                        	<td>{$platform[sys].report_platform}</td>
				<td>{$platform[sys].total}</td>
			</tr>
    		{/section}
    		</table>
                </div>
	</div>
	<div>
		<div class="title">By Product:</div>
		<div class="data">
		<table>
    		{section name=prod loop=$product}
			<tr>
                        	<td>{$product[prod].report_product}</td>
				<td>{$product[prod].total}</td>
			</tr>
    		{/section}
    		</table>
                </div>
	</div>

</div>
<br />