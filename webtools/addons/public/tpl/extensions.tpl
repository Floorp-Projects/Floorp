<h1>Extensions</h1>

<p>Extensions are small add-ons that add new functionality to
Mozilla applications. They can add anything from a toolbar
button to a completely new feature. They allow the application to be customized
to fit the personal needs of each user if they need additional features,
while minimizing the size of the application itself.</p>

<h2><a href="{$config.webpath}/rss/firefox/extensions/updated/"><img class="rss" src="{$config.webpath}/images/rss.png"/>Recently Updated</a></h2>
<ul>
{section name=ne loop=$newestExtensions step=1 start=0}
<li><a href="{$config.webpath}/{$app}/{$newestExtensions[ne].ID}/">{$newestExtensions[ne].name} {$newestExtensions[ne].version}</a> ({$newestExtensions[ne].dateupdated|date_format})</li>
{/section}
</ul>

<h2><a href="{$config.webpath}/rss/firefox/extensions/popular/"><img class="rss" src="{$config.webpath}/images/rss.png"/>Top Extensions</h2>
<ul>
{section name=pe loop=$popularExtensions step=1 start=0}
<li><a href="{$config.webpath}/{$app}/{$popularExtensions[pe].ID}/">{$popularExtensions[pe].name}</a> ({$popularExtensions[pe].dc} downloads)</li>
{/section}
</ul>
