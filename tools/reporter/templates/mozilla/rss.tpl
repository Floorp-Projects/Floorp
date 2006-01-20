<?xml version="1.0" encoding="utf-8"?>
<!-- generator="Mozilla Reporter" -->
<rss version="2.0"
	xmlns:content="http://purl.org/rss/1.0/modules/content/"
	xmlns:wfw="http://wellformedweb.org/CommentAPI/"
	xmlns:dc="http://purl.org/dc/elements/1.1/"
	>

<channel>
	<title>{$title}</title>
	<link>{$url}</link>
	<description>{description}</description>
	<pubDate>Mon, 16 Jan 2006 15:59:06 +0000</pubDate>
	<generator>Mozilla Reporter</generator>

	<language>en</language>

	{section name=mysec2 loop=$row}
	<item>
		<title>{$row[mysec2][report_id]}</title>
		<link>{$row[mysec2][report_url]}/</link>
		<pubDate>{$row[mysec2][report_file_date]}</pubDate>
	  	<category>{$row[mysec2][report_type]}</category>
		<guid isPermaLink="false">{$row[mysec2][report_url]}</guid>
		<description><![CDATA[{$row[mysec2][report_description]}]]></content:encoded>
	</item>
	{/section}
</channel>
</rss>
