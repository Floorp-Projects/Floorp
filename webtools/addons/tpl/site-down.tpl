
<h1>Site Temporarily Unavailable</h1>
<p>We are sorry, <kbd>addons.mozilla.org</kbd> is temporarily unavailable at this time.</p>

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

