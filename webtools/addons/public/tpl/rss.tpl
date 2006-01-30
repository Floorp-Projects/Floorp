<rss version="2.0">
<channel>
    <title>{$listname} :: Mozilla Addons</title>
    <link>{$config.host}{$config.webpath}</link>
    <description>Mozilla Addons is the place to get extensions and themes for your Mozilla applications.</description>
    <language>en-US</language>
    <copyright>{$smarty.now|date_format:'%Y'} The Mozilla Foundation</copyright>
    <lastBuildDate>{$smarty.now|date_format}</lastBuildDate>
    <ttl>120</ttl>
    <image>
        <title>Mozilla Addons</title>
        <link>{$config.host}{$config.webpath}</link>
        <url>http://mozilla.org/favicon.ico</url>
        <width>16</width>
        <height>16</height>
    </image>

{foreach item=row from=$data}
    <item>
        <pubDate>{$row.dateupdated}</pubDate>
        <title>{$row.title} {$row.version} for {$row.appname} </title>
        <link>{$config.host}{$config.webpath}/{$row.appname|lower}/{$row.id}/</link>
        <description>{$row.description|escape:html}</description>
    </item>
{/foreach}

</channel>
</rss>
