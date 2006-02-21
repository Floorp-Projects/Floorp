<rss version="2.0">
<channel>
    <title>{$listname|escape:html:"UTF-8"} :: Mozilla Addons</title>
    <link>{$config.host|escape:html:"UTF-8"}{$config.webpath|escape:html:"UTF-8"}</link>
    <description>Mozilla Addons is the place to get extensions and themes for your Mozilla applications.</description>
    <language>en-US</language>
    <copyright>{$smarty.now|date_format:'%Y'} The Mozilla Foundation</copyright>
    <lastBuildDate>{$smarty.now|date_format}</lastBuildDate>
    <ttl>120</ttl>
    <image>
        <title>Mozilla Addons</title>
        <link>{$config.host|escape:html:"UTF-8"}{$config.webpath|escape:html:"UTF-8"}</link>
        <url>http://mozilla.org/favicon.ico</url>
        <width>16</width>
        <height>16</height>
    </image>

{foreach item=row from=$data}
    <item>
        <pubDate>{$row.dateupdated}</pubDate>
        <title>{$row.title|escape:html:"UTF-8"} {$row.version|escape:html:"UTF-8"} for {$row.appname|escape:html:"UTF-8"} </title>
        <link>{$config.host|escape:html:"UTF-8"}{$config.webpath|escape:html:"UTF-8"}/{$row.appname|lower|escape:html:"UTF-8"}/{$row.id}/</link>
        <description>{$row.description|escape:html:"UTF-8"}</description>
    </item>
{/foreach}

</channel>
</rss>
