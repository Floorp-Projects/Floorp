CREATE TABLE IF NOT EXISTS `comments` (
  `id` int(10) NOT NULL auto_increment,
  `assoc` int(10) NOT NULL default '0',
  `owner` int(10) NOT NULL default '0',
  `time` int(15) NOT NULL default '0',
  `text` text collate utf8_unicode_ci NOT NULL,
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

CREATE TABLE IF NOT EXISTS `parties` (
  `id` int(10) NOT NULL auto_increment,
  `owner` int(10) NOT NULL default '0',
  `name` tinytext collate utf8_unicode_ci NOT NULL,
  `vname` tinytext collate utf8_unicode_ci,
  `address` tinytext collate utf8_unicode_ci NOT NULL,
  `tz` int(2) NOT NULL default '0',
  `website` text collate utf8_unicode_ci,
  `notes` text collate utf8_unicode_ci,
  `date` int(10) default NULL,
  `duration` tinyint(2) NOT NULL default '2',
  `guests` text collate utf8_unicode_ci NOT NULL,
  `confirmed` tinyint(1) NOT NULL default '0',
  `guestcomments` tinyint(1) NOT NULL default '0',
  `inviteonly` tinyint(1) NOT NULL default '0',
  `invitecode` tinytext collate utf8_unicode_ci NOT NULL,
  `lat` float default NULL,
  `long` float default NULL,
  `zoom` tinyint(2) NOT NULL default '8',
  `useflickr` tinyint(1) NOT NULL default '0',
  `flickrperms` tinyint(1) default '0',
  `flickrid` tinytext collate utf8_unicode_ci NOT NULL,
  `flickrusr` tinytext collate utf8_unicode_ci NOT NULL,
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

CREATE TABLE IF NOT EXISTS `users` (
  `id` int(10) NOT NULL auto_increment,
  `role` tinyint(2) NOT NULL default '0',
  `email` varchar(75) collate utf8_unicode_ci NOT NULL default '',
  `active` varchar(10) collate utf8_unicode_ci default NULL,
  `password` varchar(40) collate utf8_unicode_ci NOT NULL default '',
  `salt` varchar(9) collate utf8_unicode_ci NOT NULL default '',
  `name` tinytext collate utf8_unicode_ci NOT NULL,
  `location` tinytext collate utf8_unicode_ci NOT NULL,
  `tz` int(2) NOT NULL default '0',
  `website` tinytext collate utf8_unicode_ci NOT NULL,
  `lat` float default NULL,
  `long` float default NULL,
  `zoom` tinyint(2) NOT NULL default '12',
  `showemail` tinyint(1) NOT NULL default '1',
  `showloc` tinyint(1) NOT NULL default '1',
  `showmap` tinyint(1) NOT NULL default '1',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `email` (`email`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;
