{if $smarty.get.app eq "thunderbird"}
    {assign var="app" value="thunderbird"}
{elseif $smarty.get.app eq "mozilla"}
    {assign var="app" value="mozilla"}
{elseif $smarty.get.app eq "seamonkey"}
    {assign var="app" value="mozilla"}
{else}
    {assign var="app" value="firefox"}
{/if}
{if $app eq "firefox"}
		</div><!-- end #maincontent div -->
	
	</div><!-- end #content div -->
	
	<div id="disclaimer">
		<p>Mozilla is providing links to these applications as a courtesy, and makes no representations regarding the applications or any information related there to. Any questions, complaints or claims regarding the applications must be directed to the appropriate software vendor. See our <a href="#">Support Page</a> for support information and contacts.</p>
	</div>

</div>

</body>
</html>
{else}
{include file="inc/wrappers/default-footer.tpl"}
{/if}