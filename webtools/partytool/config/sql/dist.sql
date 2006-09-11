CREATE TABLE IF NOT EXISTS `comments` (
  `id` int(10) NOT NULL auto_increment,
  `assoc` int(10) NOT NULL default '0',
  `owner` int(10) NOT NULL default '0',
  `time` int(15) NOT NULL default '0',
  `text` text NOT NULL,
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

CREATE TABLE IF NOT EXISTS `parties` (
  `id` int(10) NOT NULL auto_increment,
  `owner` int(10) NOT NULL default '0',
  `name` tinytext NOT NULL,
  `vname` tinytext,
  `address` tinytext NOT NULL,
  `website` tinytext,
  `notes` text,
  `date` int(10) default NULL,
  `duration` int(11) default NULL,
  `guests` tinytext NOT NULL,
  `confirmed` tinyint(1) NOT NULL default '0',
  `inviteonly` tinyint(1) NOT NULL default '0',
  `lat` float default NULL,
  `long` float default NULL,
  `zoom` int(5) default NULL,
  `useflickr` tinyint(1) NOT NULL default '0',
  `flickrid` tinytext NOT NULL,
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

CREATE TABLE IF NOT EXISTS `users` (
  `id` int(10) NOT NULL auto_increment,
  `role` tinyint(2) NOT NULL default '0',
  `email` varchar(75) NOT NULL default '',
  `active` varchar(10) default NULL,
  `password` varchar(40) NOT NULL default '',
  `salt` varchar(9) NOT NULL default '',
  `name` tinytext NOT NULL,
  `location` tinytext NOT NULL,
  `website` tinytext NOT NULL,
  `lat` float default NULL,
  `long` float default NULL,
  `zoom` tinyint(2) NOT NULL default '12',
  `showemail` tinyint(1) NOT NULL default '1',
  `showloc` tinyint(1) NOT NULL default '1',
  `showmap` tinyint(1) NOT NULL default '1',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `email` (`email`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;
