<script src="https://www.stage.mozilla.com/js/download.js"></script>
<script src="{$config.webpath}/js/dictionaries.js"></script>

<div id="mBody">
<h1>Dictionaries</h1>

<p>These dictionaries are available for use with Firefox's built-in spell-checking.</p>

<script>writeCurrentDictionary();</script>

<script>
document.writeln("<table>");
{section name=d loop=$dicts step=1 start=1}
addDictionary("{$dicts[d].code}", "{$dicts[d].uri}");
{/section}
document.writeln("</table>");
</script>

<noscript>
<ul>
{section name=d loop=$dicts step=1 start=0}
  <li><a href="{$dicts[d].uri}">{$dicts[d].code}</a></li>
{/section}
</ul>
</noscript>
