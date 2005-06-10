--
-- Database: `reporter`
--
CREATE DATABASE `reporter`;
USE reporter;

-- --------------------------------------------------------

--
-- Table structure for table `host`
--

CREATE TABLE `host` (
  `host_id` varchar(32) NOT NULL default '',
  `host_hostname` varchar(255) NOT NULL default '',
  `host_date_added` datetime NOT NULL default '0000-00-00 00:00:00',
  `host_user_added` varchar(60) NOT NULL default '',
  PRIMARY KEY  (`host_id`),
  KEY `host_hostname` (`host_hostname`)
) TYPE=MyISAM;

-- --------------------------------------------------------

-- 
-- Table structure for table `product`
-- 

CREATE TABLE `product` (
  `product_id` varchar(150) NOT NULL default '',
  `product_description` varchar(150) NOT NULL default ''
) TYPE=MyISAM;

-- --------------------------------------------------------

-- 
-- Table structure for table `report`
-- 

CREATE TABLE `report` (
  `report_id` varchar(17) NOT NULL default '',
  `report_url` varchar(255) NOT NULL default '',
  `report_host_id` varchar(32) NOT NULL default '',
  `report_problem_type` varchar(5) NOT NULL default '0',
  `report_description` tinytext NOT NULL,
  `report_behind_login` int(11) NOT NULL default '0',
  `report_useragent` varchar(255) NOT NULL default '',
  `report_platform` varchar(20) NOT NULL default '',
  `report_oscpu` varchar(100) NOT NULL default '',
  `report_language` varchar(7) NOT NULL default '',
  `report_gecko` varchar(8) NOT NULL default '',
  `report_buildconfig` tinytext NOT NULL,
  `report_product` varchar(100) NOT NULL default '',
  `report_email` varchar(255) NOT NULL default '',
  `report_ip` varchar(15) NOT NULL default '',
  `report_file_date` datetime NOT NULL default '0000-00-00 00:00:00',
  `report_sysid` varchar(10) NOT NULL default '',
  PRIMARY KEY  (`report_id`),
  KEY `report_host_id` (`report_host_id`)
) TYPE=MyISAM;

-- --------------------------------------------------------

-- 
-- Table structure for table `sysid`
--

CREATE TABLE `sysid` (
  `sysid_id` varchar(10) binary NOT NULL default '',
  `sysid_created` datetime NOT NULL default '0000-00-00 00:00:00',
  `sysid_created_ip` varchar(15) binary NOT NULL default '',
  `sysid_language` varchar(7) binary NOT NULL default '',
  PRIMARY KEY  (`sysid_id`)
) TYPE=MyISAM;

-- --------------------------------------------------------

-- 
-- Table structure for table `user`
-- 

CREATE TABLE `user` (
  `user_id` int(11) NOT NULL default '0',
  `user_username` varchar(50) NOT NULL default '',
  `user_password` varchar(40) NOT NULL default '',
  `user_realname` varchar(25) NOT NULL default '',
  `user_email` varchar(255) NOT NULL default '',
  `user_added_by` tinytext NOT NULL,
  `user_added_datetime` datetime NOT NULL default '0000-00-00 00:00:00',
  `user_last_login` datetime NOT NULL default '0000-00-00 00:00:00',
  `user_last_ip_address` varchar(16) NOT NULL default '',
  `user_status` int(11) NOT NULL default '0',
  PRIMARY KEY  (`user_id`),
  KEY `user_password` (`user_password`),
  KEY `user_id` (`user_id`)
) TYPE=MyISAM;
