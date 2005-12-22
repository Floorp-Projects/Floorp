
<h1>Error Found</h1>
<p>The was an error processing your request.  Please check your inputs and try again.</p>
<dl>
{if $error}
<dt>Error Message</dt>
<dd>{$error}</dd>
{/if}
<dt>Backtrace</dt>
<dd><pre>
{foreach name=rows item=row from=$backtrace}
{foreach name=row key=key item=item from=$row}
{$key}: {$item}
{/foreach}
{/foreach}
</pre></dd>
</dl>

