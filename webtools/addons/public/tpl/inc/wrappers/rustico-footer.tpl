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
	
  </div><!-- end #container div -->

<div id="footer">
  <div class="compact-list">

  <p id="tool-links"><a href="{$config.webpath}/developers/" class="switch-devcp">Developer Login</a> <a href="{$config.webpath}/login.php">Log In</a> <a href="{$config.webpath}/logout.php">Logout</a> <a href="{$config.webpath}/createaccount.php">Register</a> </p>
    
  <p id="doc-links"><a href="{$config.webpath}/faq.php">FAQ</a> <a href="{$config.webpath}/feeds.php">Feeds/RSS</a></p>

<p id="switch-links"><a href="{$config.webpath}/thunderbird/" class="switch-tb">Thunderbird Add-ons </a><a href="{$config.webpath}/mozilla/" class="switch-suite">Mozilla Suite Add-ons </a></p>

</div>

	<div id="disclaimer">
		<p>Mozilla is providing links to these applications as a courtesy, and makes no representations regarding the applications or any information related there to. Any questions, complaints or claims regarding the applications must be directed to the appropriate software vendor. See our <a href="http://www.mozilla.com/support/">Support Page</a> for support information and contacts.</p>
	</div>
</div>

</body>
</html>
{else}
{include file="inc/wrappers/default-footer.tpl"}
{/if}