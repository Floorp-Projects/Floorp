
<ul id="nav">
<li><strong><a href="{$config.webpath}/search.php?app={$app}&amp;appfilter={$app}&amp;type={$type}&amp;sort=newest">Newest</a></strong></li>
<li><strong><a href="{$config.webpath}/search.php?app={$app}&amp;appfilter={$app}&amp;type={$type}&amp;sort=rating">Top Rated</a></strong></li>
<li><strong><a href="{$config.webpath}/search.php?app={$app}&amp;appfilter={$app}&amp;type={$type}&amp;sort=downloads">Popular</a></strong></li>
<li><strong><a href="{$config.webpath}/search.php?app={$app}&amp;appfilter={$app}&amp;type={$type}">All Categories</a</strong>
<ul>
    {foreach key=id item=name from=$cats}
    <li><a href="{$config.webpath}/search.php?cat={$id}&amp;app={$app}&amp;appfilter={$app}&amp;type={$type}">{$name}</a></li>
    {/foreach}
    </li>
</ul>
</ul>

