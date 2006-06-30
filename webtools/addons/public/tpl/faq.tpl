<h1>Frequently Asked Questions</h1>

<h2>What is Mozilla Update?</h2>
<p>Mozilla Update is the place to get updates and extras for
your <a href="http://www.mozilla.org/">Mozilla</a> products.  This service
has undergone <a href="./update.php">several changes</a> that we hope
will make the site better.  We have re-enabled access to the developers area
and look forward to serving the extension and theme developer community in the
future!  We will be posting frequent 
<a href="./update.php">status updates</a> as to our progress with the 
UMO service.  The best is yet to come!</p>

<h2>How do I get involved?</h2>
<p class="first">We are looking for volunteers to help us with UMO. We are in need of PHP
developers to help with redesigning the site, and people to review extensions
and themes that get submitted to UMO. We especially need Mac and Thunderbird
users. If you are interested in being a part of this exciting project, please
join us in <kbd>#addons</kbd> on <kbd>irc.mozilla.org</kbd> to start getting a feeling for what's up or for a more informal chat.
</p>

<h2>What can I find here?</h2>
<dl>
<dt>Extensions</dt>
<dd>Extensions are small add-ons that add new functionality to your Firefox
web browser or Thunderbird email client.  They can add anything from
toolbars to completely new features.  Browse extensions for:
<a href="{$config.webpath}/firefox/extensions/">Firefox</a>, 
<a href="{$config.webpath}/thunderbird/extensions/">Thunderbird</a>,
<a href="{$config.webpath}/mozilla/extensions/">Mozilla Suite</a>
</dd>

<dt>Themes</dt>
<dd>Themes allow you to change the way your Mozilla program looks. 
New graphics and colors. Browse themes for: 
<a href="{$config.webpath}/firefox/themes/">Firefox</a>, 
<a href="{$config.webpath}/thunderbird/themes/">Thunderbird</a>,
<a href="{$config.webpath}/mozilla/themes/">Mozilla Suite</a>
</dd>

<dt>Plugins</dt>
<dd>Plugins are programs that also add funtionality to your browser to
deliver specific content like videos, games, and music.  Examples of Plugins
are Macromedia Flash Player, Adobe Acrobat, and Sun Microsystem's Java
Software.  Browse plug-ins for:
<a href="{$config.webpath}/{$app}/plugins/">Firefox &amp; Mozilla Suite</a>
</dd>
	</dl>

<dl>
{section name=faq loop=$faq}
<dt>{$faq[faq].title}</dt>
<dd>{$faq[faq].text|nl2br}</dd>
{/section}
</dl>

<h2>Valid App Versions for Addon Developers</h2>

<table class="appversions">
<tr>
    <th>Application Name</th>
    <th>Version</th>
    <th>GUID</th>
</tr>

{foreach item=app from=$appVersions}
<tr>
<td>{$app.appName}</td>
<td>{$app.displayVersion}</td>
<td>{$app.guid}</td></tr>
{/foreach}
</table>


<h2>I see this error when trying to upload my extension or theme: "The Name for your extension or theme already exists in the Update database."</h2>
<p>This is typically caused by mismatching GUIDs or a duplicate record.  If there is a duplicate record, chances are you should submit an update instead of trying to create a new extension or theme.  If you cannot see the existing record, then it is owned by another author, and you should consider renaming your extension/theme.</p>
