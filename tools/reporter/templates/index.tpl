<fieldset>
	<legend>Look up Report</legend>
	<form method="get" action ="{$app_url}/report/" id="getReport">
		<p>
			<label for="report_id">Report ID</label>
			<input type="text" id="report_id" name="report_id" />
			<input type="submit" id="submit_reportID" name="submit_reportID" value="Lookup Report" />
		</p>
	</form>
</fieldset>

<!-- Query -->
<fieldset>
	<legend>Search for a Report</legend>
	<form method="get" action="{$app_url}/query/" id="detailQuery">
	<table id="reporterSearch">
		<tr>
			<td class="label"><label for="report_description">Description:</label></td>
			<td><input id="report_description" name="report_description" type="text" size="35" value="{$report_description}"></td>
			<td rowspan="5">
				<table>
					<tr>
						<td class="label"><label for="report_useragent">User Agent:</label></td>
						<td><input id="report_useragent" name="report_useragent" type="text" size="35" value="{$report_useragent}"></td>
					</tr>
					<tr>
						<td class="label"><label for="report_gecko">Gecko Version:</label></td>
						<td><input id="report_gecko" name="report_gecko" type="text" value="{$report_gecko}"></td>
					</tr>
					<tr>
						<td class="label"><label for="report_language">Language (ex. en-US):</label></td>
						<td><input id="report_language" name="report_language" type="text" value="{$report_language}"></td>
					</tr>
					<tr>
						<td class="label"><label for="report_platform">Platform:</label></td>
						<td>
							<table>
								<tr>
									<td>
										<input type="radio" name="report_platform" id="AllPlatforms" value="" {if $report_platform == '' || $report_platform == null}checked="checked"{/if}><label for="AllPlatforms">All</label><br>
										<input type="radio" name="report_platform" id="Win32" value="Win32" {if $report_platform == 'Win32'}checked="checked"{/if}><label for="Win32">Windows (32)</label><br>
										<input type="radio" name="report_platform" id="Windows" value="Windows" {if $report_platform == 'Windows'}checked="checked"{/if}><label for="Windows">Windows (64 Bit)</label><br>
										<input type="radio" name="report_platform" id="MacPPC" value="MacPPC" {if $report_platform == 'MacPPC'}checked="checked"{/if}><label for="MacPPC">MacOS X</label><br>
										<input type="radio" name="report_platform" id="X11" value="X11" {if $report_platform == 'X11'}checked="checked"{/if}><label for="X11">X11</label><br>
									</td><td>
										<input type="radio" name="report_platform" id="OS2" value="OS/2" {if $report_platform == 'OS/2'}checked="checked"{/if}><label for="OS2">OS/2</label><br>
										<input type="radio" name="report_platform" id="Photon" value="Photon" {if $report_platform == 'Photon'}checked="checked"{/if}><label for="Photon">Photon</label><br>
										<input type="radio" name="report_platform" id="BeOS" value="BeOS" {if $report_platform == 'BeOS'}checked="checked"{/if}><label for="BeOS">BeOS</label><br>
										<input type="radio" name="report_platform" id="unknown" value="?" {if $report_platform == '?'}checked="checked"{/if}><label for="unknown">unknown</label><br>
									</td>
								</tr>
							</table>
						</td>
					</tr>
					<tr>
						<td class="label"><label for="report_oscpu" title="oscpu">OS:</label></td>
						<td><input id="report_oscpu" name="report_oscpu" type="text" size="35" value="{$report_oscpu}"></td>
					</tr>
					<tr>
						<td class="label"><label for="report_product">Product:</label></td>
						<td>
							<select name="report_product" id="report_product">
								<option value="-1">Any</option>
								{html_options values=$product_options output=$product_options selected=$report_product}
 							</select>
						</td>
					</tr>
					<tr>
						<td class="label"><label for="report_file_date_start">File Date starts:</label></td>
						<td><input id="report_file_date_start" name="report_file_date_start" value="{if $report_file_date_start != null}{$report_file_date_start}{else}YYYY-MM-DD{/if}" {literal}onfocus="if(this.value=='YYYY-MM-DD'){this.value=''}"{/literal} type="text"></td>
					</tr>
					<tr>
						<td class="label"><label for="report_file_date_end">ends:</label></td>
						<td><input id="report_file_date_end" name="report_file_date_end" value="{if $report_file_date_end != null}{$report_file_date_end}{else}YYYY-MM-DD{/if}" {literal}onfocus="if(this.value=='YYYY-MM-DD'){this.value=''}"{/literal} type="text"></td>
					</tr>
					<tr>
						<td class="label"><label for="show">Results Per Page:</label></td>
						<td><input id="show" name="show" value="{if $show != null}{$show}{else}25{/if}" type="text"></td>
					</tr>
					<tr>
						<td class="label"><label for="count">Get Aggregate:</label></td>
						<td><input type="checkbox" id="count" name="count"></td>
					</tr>
				</table>
			</td>
		</tr>
		<tr>
			<td class="label"><label for="host_hostname">Host:</label></td>
			<td><input id="host_hostname" name="host_hostname" type="text" size="35" value="{$host_hostname}"></td>
		</tr>
		<tr>
			<td class="label"><label for="report_problem_type">Problem Type:</label></td>
			<td>
				<input type="radio" name="report_problem_type" id="null" value="-1" {if $report_problem_type == '-1' || $report_problem_type == null}checked="checked"{/if}><label for="null">All types</label><br>
				{html_radios name="report_problem_type" options=$problem_types selected=$report_problem_type separator="<br />"}
			</td>
		</tr>
		<tr>
			<td class="label">Site requires login:</td>
			<td>
				<input type="radio" name="report_behind_login" id="behind_login_null" value="-1" {if $report_behind_login == '-1' || $report_behind_login == ''}checked="checked"{/if}><label for="behind_login_null">Any</label>
				<input type="radio" name="report_behind_login" id="behind_login_yes" value="1" {if $report_behind_login == '1'}checked="checked"{/if}><label for="behind_login_yes">Yes</label>
				<input type="radio" name="report_behind_login" id="behind_login_no" value="0" {if $report_behind_login == '0'}checked="checked"{/if}><label for="behind_login_no">No</label>
			</td>
		</tr>
		<tr>
			<td class="label"><label for="selected[]">View:</label></td>
			<td>
				<select name="selected[]" id="selected[]" multiple="multiple" size="5">
					{html_options options=$selected_options selected=$selected}
				</select>
			</td>
		</tr>
		<tr>
			<td></td>
			<td colspan="2">
				<input id="submit_query" name="submit_query" value="Search" type="submit">
			</td>
		</tr>
	</table>
	</form>
</fieldset>
<br /><br />
<div id="reporter_note">
	<h3>Wildcards</h3>
	<p><em>Description</em>, <em>Hostname</em>, <em>Useragent</em> support wildcards.</p>
	<p><code>%</code> will search for 0+ characters, <code>_</code> (underscore) is for a single character.</p>
</div>

