<script src="https://www.mozilla.com/js/download.js"></script>
<script src="{$config.webpath}/js/dictionaries.js"></script>
<script>
{section name=d loop=$dicts step=1 start=1}
addDictionary("{$dicts[d].code}", "{$dicts[d].uri}", {$dicts[d].size});
{/section}
</script>

<div id="mBody">
<h1>Dictionaries</h1>

<p>These dictionaries work with the spell-checking feature in Firefox 2.</p>

<script>writeCurrentDictionary();</script>

<h2>All dictionaries</h2>

<script>
document.writeln("<table class='dalvay-table'>");
writeAllDictionaries();
document.writeln("</table>");
</script>

<noscript>
<ul>
{section name=d loop=$dicts step=1 start=0}
  <li><a href="{$dicts[d].uri}">{$dicts[d].code}</a> {$dicts[d].size)KB)</li>
{/section}
</ul>
</noscript>
