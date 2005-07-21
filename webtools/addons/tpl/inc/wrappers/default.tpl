{include file="inc/header.tpl"}

<hr class="hide">

{if $systemMessage}
<div class="key-point">
<h2>{$systemMesage.header}</h2>
<p>{$systemMessage.text|nl2br}</p>
</div>
{/if}

<div id="mBody">


 {* sidebar gets included if set *}
 {if $sidebar}
 <div id="side">
 {include file=$sidebar}
 </div>
 {/if}

<div id="mainContent">
{if $content}
{include file=$content}
{else}
<p>Error loading content.</p>
{if $error}
<div id="error">{$error}</div>
{/if}
<p><a href="mailto:umo-admins@mozilla.org?Subject=Broken page at {$smarty.server.PHP_SELF}">Report this problem</a></p>
{/if}
</div>
<!-- close mainContent -->

<hr class="hide">

{include file="inc/footer.tpl"}

</div>
<!-- close mBody -->

</div>
<!-- close container -->

</body>
</html>
