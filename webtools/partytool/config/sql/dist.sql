CREATE TABLE `comments` (
  `id` int(10) NOT NULL auto_increment,
  `assoc` int(10) NOT NULL default '0',
  `owner` int(10) NOT NULL default '0',
  `time` int(15) NOT NULL default '0',
  `text` text collate utf8_unicode_ci NOT NULL,
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

CREATE TABLE `guests` (
  `id` int(10) NOT NULL auto_increment,
  `pid` int(10) NOT NULL default '0',
  `uid` int(10) NOT NULL default '0',
  `invited` tinyint(1) NOT NULL default '1',
  PRIMARY KEY  (`id`),
  KEY `pid` (`pid`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

CREATE TABLE `pages` (
  `id` int(10) NOT NULL auto_increment,
  `text` text collate utf8_unicode_ci NOT NULL,
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

INSERT INTO `pages` (`id`, `text`) VALUES (1, '&lt;h2&gt;Join the Fun!&lt;/h2&gt;\n&lt;p&gt;All over the world, we&#39;re celebrating the launch of Firefox 2. Join the fun by hosting or attending a party. We&#39;re targeting the weekend of October 27th for the shared celebration, but if you&#39;re hosting, you make the call.&lt;/p&gt;\n\n&lt;p&gt;To join the fun, &lt;a href=&quot;/users/register&quot;&gt;register&lt;/a&gt; for a Firefox Party account, and sign up to host or attend.&lt;/p&gt;\n\n&lt;p style=&quot;border: 1px solid #555; background: #faffd4; padding: 5px; font&#45;weight: bold&quot;&gt;Be one of the first 50 party hosts registered and get three extra launch exclusive t&#45;shirts with your purchase of the &lt;a href=&quot;&quot;&gt;Firefox 2 Party Pack&lt;/a&gt;.  We&#39;re selling the party packs and shirts at cost, so it&#39;s a great deal, and for parties with unusually large attendance, we&#39;ll be sending out additional swag for door prizes and other give&#45;aways. Stay tuned for updates!&lt;/p&gt;');
INSERT INTO `pages` (`id`, `text`) VALUES (2, '1162007940');

CREATE TABLE `parties` (
  `id` int(10) NOT NULL auto_increment,
  `owner` int(10) NOT NULL default '0',
  `name` tinytext collate utf8_unicode_ci NOT NULL,
  `vname` tinytext collate utf8_unicode_ci NOT NULL,
  `address` tinytext collate utf8_unicode_ci NOT NULL,
  `tz` int(2) NOT NULL default '0',
  `website` text collate utf8_unicode_ci NOT NULL,
  `notes` text collate utf8_unicode_ci NOT NULL,
  `date` int(10) NOT NULL default '0',
  `duration` tinyint(2) NOT NULL default '2',
  `confirmed` tinyint(1) NOT NULL default '1',
  `canceled` tinyint(1) NOT NULL default '0',
  `guestcomments` tinyint(1) NOT NULL default '0',
  `inviteonly` tinyint(1) NOT NULL default '0',
  `invitecode` tinytext collate utf8_unicode_ci NOT NULL,
  `lat` float NOT NULL default '0',
  `long` float NOT NULL default '0',
  `zoom` tinyint(2) NOT NULL default '1',
  `useflickr` tinyint(1) NOT NULL default '0',
  `flickrperms` tinyint(1) NOT NULL default '0',
  `flickrid` tinytext collate utf8_unicode_ci NOT NULL,
  `flickrusr` tinytext collate utf8_unicode_ci NOT NULL,
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

CREATE TABLE `sessions` (
  `id` varchar(255) character set latin1 NOT NULL default '',
  `data` text character set latin1,
  `expires` int(11) default NULL,
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

CREATE TABLE `users` (
  `id` int(10) NOT NULL auto_increment,
  `role` tinyint(1) NOT NULL default '0',
  `email` tinytext collate utf8_unicode_ci NOT NULL,
  `active` varchar(10) collate utf8_unicode_ci NOT NULL default '0',
  `password` varchar(75) collate utf8_unicode_ci NOT NULL default '',
  `salt` varchar(9) collate utf8_unicode_ci NOT NULL default '',
  `name` tinytext collate utf8_unicode_ci NOT NULL,
  `location` tinytext collate utf8_unicode_ci NOT NULL,
  `tz` tinyint(2) NOT NULL default '0',
  `website` text collate utf8_unicode_ci NOT NULL,
  `lat` float NOT NULL default '0',
  `long` float NOT NULL default '0',
  `zoom` tinyint(2) NOT NULL default '1',
  `showemail` tinyint(1) NOT NULL default '0',
  `showloc` tinyint(1) NOT NULL default '1',
  `showmap` tinyint(1) NOT NULL default '1',
  UNIQUE KEY `email` (`email`)
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;
