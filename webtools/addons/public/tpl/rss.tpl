<?xml-stylesheet type="text/xsl" href="{$config.webpath|escape:html:"UTF-8"}/css/rss.xsl" media="screen"?>
<rss version="2.0">
<channel>
    <title>{$list|escape:html:"UTF-8"} Mozilla Add-ons</title>
    <link>{$config.host|escape:html:"UTF-8"}{$config.webpath|escape:html:"UTF-8"}</link>
    <description>Mozilla Add-ons is the place to get extensions and themes for your Mozilla applications.</description>
    <language>en-US</language>
    <copyright>{$smarty.now|date_format:'%Y'} The Mozilla Foundation</copyright>
    <lastBuildDate>{$smarty.now|date_format:"%a, %d %b %Y %H:%M:%S %Z"}</lastBuildDate>
    <ttl>120</ttl>
    <image>
        <title>Mozilla Add-ons</title>
        <link>{$config.host|escape:html:"UTF-8"}{$config.webpath|escape:html:"UTF-8"}</link>
        <url>http://mozilla.org/images/mozilla-16.png</url>
        <width>16</width>
        <height>16</height>
    </image>

{foreach item=row from=$data}
    <item>
        <pubDate>{$row.dateupdated|date_format:"%a, %d %b %Y %H:%M:%S %Z"}</pubDate>
        <title>{$row.title|escape:html:"UTF-8"} {$row.version|escape:html:"UTF-8"} for {$row.appname|escape:html:"UTF-8"} </title>
        <link>{$config.host|escape:html:"UTF-8"}{$config.webpath|escape:html:"UTF-8"}/{$row.appname|lower|escape:html:"UTF-8"}/{$row.id}/</link>
        <description>{$row.description|escape:html:"UTF-8"}</description>
        <guid>{$config.host|escape:html:"UTF-8"}{$config.webpath|escape:html:"UTF-8"}/{$row.appname|lower|escape:html:"UTF-8"}/{$row.id}/</guid>
    </item>
{/foreach}

</channel>
</rss>
