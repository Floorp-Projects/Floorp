<h2><strong>{$user->UserName}</strong> &raquo; Author Profile</h2>
<dl>
<dt>Homepage</dt>
<dd><a href="{$user->UserWebsite|regex_replace:'/^([^(http:\/\/|https:\/\/)].+)$/':'http://\1'}">{$user->UserWebsite}</a></dd>

<dt>Email</dt>
<dd>
{if $user->UserEmailHide eq 1}
Email not disclosed
{else}
<a href="mailto:{$user->UserEmail|escape}">{$user->UserEmail|escape}</a>
{/if}
</dd>

<dt>Last Logged In</dt>
<dd>{$user->UserLastLogin|date_format:"%A, %B %e, %Y"}</dd>
</dl>

<h3>Add-ons by {$user->UserName}</h3>
<dl>
{section name=aa loop=$user->AddOns}
<dt><a href="{$config.webpath}/{$app}/{$user->AddOns[aa].ID}/">{$user->AddOns[aa].Name}</a></dt>
<dd>{$user->AddOns[aa].Description|nl2br}</dd>
</dl>
{/section}
