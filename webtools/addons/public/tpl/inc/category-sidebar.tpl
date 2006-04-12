
<ul id="nav">
<li><span>Browse by Category</span></li>
{foreach key=id item=name from=$cats}
<li><a href="{$config.webpath}/search.php?cat={$id}&amp;app={$app}&amp;appfilter={$app}">{$name}</a></li>
{/foreach}
</ul>

