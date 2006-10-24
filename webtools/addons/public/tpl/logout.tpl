{if $app eq "firefox"}
<div id="mBody">
<h1 class="first">Logged out</h1>
<p>You are now logged out.  Bye!</p>
</div>
{else}
<div id="mBody">
<h1 class="first">Logout</h1>
<p>Your login information has been forgotten.</p>
<p><strong><a href="{$config.webpath}/{$app}/">Return to Home &raquo;</a></strong></p>
</div>
{/if}