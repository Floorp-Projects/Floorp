-- SQL specific to v2 needed for migration of v1 production database
-- --------------------------------------------------------

-- 
-- Table structure for table `session_data`
-- 

DROP TABLE IF EXISTS `session_data`;
CREATE TABLE `session_data` (
  `sess_id` varchar(255) NOT NULL default '',
  `sess_user_id` int(11) NOT NULL default '0',
  `sess_expires` int(11) unsigned NOT NULL default '0',
  `sess_data` text,
  PRIMARY KEY  (`sess_id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;


