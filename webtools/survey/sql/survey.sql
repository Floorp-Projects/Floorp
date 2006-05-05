-- --------------------------------------------------------

-- 
-- Table structure for table `app`
-- 

CREATE TABLE `app` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `app_name` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

-- 
-- Dumping data for table `app`
-- 

INSERT INTO `app` VALUES (1, 'Mozilla Firefox');
INSERT INTO `app` VALUES (2, 'Mozilla Thunderbird');

-- --------------------------------------------------------

-- 
-- Table structure for table `intend`
-- 

CREATE TABLE `intend` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `app_id` int(10) unsigned NOT NULL default '0',
  `description` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  `pos` int(10) unsigned NOT NULL default '0',
  PRIMARY KEY  (`id`),
  KEY `app_id` (`app_id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

-- 
-- Dumping data for table `intend`
-- 

INSERT INTO `intend` VALUES (1, 1, 'To fully replace my previous web browser', 0);
INSERT INTO `intend` VALUES (2, 1, 'To use in addition to another web browser', 0);
INSERT INTO `intend` VALUES (3, 1, 'I just wanted to try it out', 0);
INSERT INTO `intend` VALUES (5, 2, 'To manage e-mail for an ISP account', 1500);
INSERT INTO `intend` VALUES (6, 2, 'To manage e-mail in a corporate environment', 105);
INSERT INTO `intend` VALUES (7, 2, 'To manage e-mail from a webmail account (gmail, yahoo, etc)', 90);
INSERT INTO `intend` VALUES (8, 2, 'I just wanted to try it out', 2);

-- --------------------------------------------------------

-- 
-- Table structure for table `issue`
-- 

CREATE TABLE `issue` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `app_id` int(10) unsigned NOT NULL default '0',
  `description` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  `pos` int(10) unsigned NOT NULL default '0',
  PRIMARY KEY  (`id`),
  KEY `app_id` (`app_id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

-- 
-- Dumping data for table `issue`
-- 

INSERT INTO `issue` VALUES (1, 1, 'Some web pages wouldn''t work', 200);
INSERT INTO `issue` VALUES (2, 1, 'Plugin compatibility (Flash, Adobe Acrobat, Windows Media Player, etc.)', 0);
INSERT INTO `issue` VALUES (3, 1, 'Performance (load delays, memory usage)', 0);
INSERT INTO `issue` VALUES (4, 1, 'Hard to use/confusing (menus, display, etc.)', 0);
INSERT INTO `issue` VALUES (5, 1, 'Missing features', 0);
INSERT INTO `issue` VALUES (6, 1, 'Security', 0);
INSERT INTO `issue` VALUES (7, 1, 'Some features didn''t work', 0);
INSERT INTO `issue` VALUES (8, 1, 'Printing', 0);
INSERT INTO `issue` VALUES (9, 1, 'Just temporary, I''m planning to install Firefox again soon', 201);
INSERT INTO `issue` VALUES (10, 2, 'Just temporary, I''m planning to install Thunderbird again soon', 50);
INSERT INTO `issue` VALUES (11, 2, 'Lack of calendaring support', 40);
INSERT INTO `issue` VALUES (12, 2, 'Performance (long time to start up, etc.)', 30);
INSERT INTO `issue` VALUES (13, 2, 'Unable to read my mail', 20);
INSERT INTO `issue` VALUES (14, 2, 'Missing features', 10);

-- --------------------------------------------------------

-- 
-- Table structure for table `issue_result_map`
-- 

CREATE TABLE `issue_result_map` (
  `result_id` bigint(20) unsigned NOT NULL default '0',
  `issue_id` int(10) unsigned NOT NULL default '0',
  `other` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  PRIMARY KEY  (`result_id`,`issue_id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

-- --------------------------------------------------------

-- 
-- Table structure for table `result`
-- 

CREATE TABLE `result` (
  `id` bigint(20) unsigned NOT NULL auto_increment,
  `intend_id` int(10) unsigned NOT NULL default '0',
  `intend_text` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  `product` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  `useragent` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  `http_user_agent` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  `comments` text collate utf8_unicode_ci NOT NULL,
  `date_submitted` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;
