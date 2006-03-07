-- --------------------------------------------------------

-- 
-- Table structure for table `intend`
-- 

CREATE TABLE `intend` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `description` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  `pos` int(10) unsigned NOT NULL default '0',
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci AUTO_INCREMENT=5 ;

-- 
-- Dumping data for table `intend`
-- 

INSERT INTO `intend` VALUES (1, 'To fully replace my previous web browser', 0);
INSERT INTO `intend` VALUES (2, 'To use in addition to another web browser', 0);
INSERT INTO `intend` VALUES (3, 'I just wanted to try it out', 0);

-- --------------------------------------------------------

-- 
-- Table structure for table `issue`
-- 

CREATE TABLE `issue` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `description` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  `pos` int(10) unsigned NOT NULL default '0',
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci AUTO_INCREMENT=10 ;

-- 
-- Dumping data for table `issue`
-- 

INSERT INTO `issue` VALUES (1, 'Some web pages wouldn''t work', 200);
INSERT INTO `issue` VALUES (2, 'Plugin compatibility (Flash, Adobe Acrobat, Windows Media Player, etc.)', 0);
INSERT INTO `issue` VALUES (3, 'Performance (load delays, memory usage)', 0);
INSERT INTO `issue` VALUES (4, 'Hard to use/confusing (menus, display, etc.)', 0);
INSERT INTO `issue` VALUES (5, 'Missing features', 0);
INSERT INTO `issue` VALUES (6, 'Security', 0);
INSERT INTO `issue` VALUES (7, 'Some features didn''t work', 0);
INSERT INTO `issue` VALUES (8, 'Printing', 0);
INSERT INTO `issue` VALUES (9, 'Just temporary, I''m planning to install Firefox again soon', 201);

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
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci AUTO_INCREMENT=8 ;
