<script src="https://www.stage.mozilla.com/js/download.js"></script>

<div id="mBody">
<h1>Dictionaries</h1>

<p>These dictionaries are available for use with Firefox's built-in spell-checking.</p>

<ul>
{section name=d loop=$dicts step=1 start=0}
  <li><a href="{$dicts[d].uri}">{$dicts[d].code}</a></li>
{/section}
</ul>
