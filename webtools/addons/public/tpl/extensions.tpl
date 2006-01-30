<h1>Extensions</h1>

<p>Extensions are small add-ons that add new functionality to
Mozilla applications. They can add anything from a toolbar
button to a completely new feature. They allow the application to be customized
to fit the personal needs of each user if they need additional features,
while minimizing the size of the application itself.</p>

<h2>Fresh from the Oven</h2>
<ul>
{section name=ne loop=$newestExtensions step=1 start=0}
<li><a href="{$config.webpath}/{$app}/{$newestExtensions[ne].ID}/">{$newestExtensions[ne].name}</a> ({$newestExtensions[ne].dateupdated|date_format})</li>
{/section}
</ul>

<h2>Hot this Week</h2>
<ul>
{section name=pe loop=$popularExtensions step=1 start=0}
<li><a href="{$config.webpath}/{$app}/{$popularExtensions[pe].ID}/">{$popularExtensions[pe].name}</a> ({$popularExtensions[pe].dc} downloads)</li>
{/section}
</ul>
