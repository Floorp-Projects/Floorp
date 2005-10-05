-- MySQL dump 10.9
--
-- Host: localhost    Database: litmus_staging
-- ------------------------------------------------------
-- Server version	4.1.10a-standard

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;

--
-- Current Database: `litmus_staging`
--

CREATE DATABASE /*!32312 IF NOT EXISTS*/ `litmus_staging` /*!40100 DEFAULT CHARACTER SET latin1 COLLATE latin1_bin */;

USE `litmus_staging`;

--
-- Table structure for table `branches`
--

DROP TABLE IF EXISTS `branches`;
CREATE TABLE `branches` (
  `branch_id` smallint(6) NOT NULL auto_increment,
  `product_id` tinyint(4) NOT NULL default '0',
  `name` varchar(64) collate latin1_bin NOT NULL default '',
  `detect_regexp` varchar(255) collate latin1_bin default NULL,
  PRIMARY KEY  (`branch_id`),
  KEY `product_id` (`product_id`),
  KEY `name` (`name`),
  KEY `detect_regexp` (`detect_regexp`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--
-- Table structure for table `build_type_lookup`
--

DROP TABLE IF EXISTS `build_type_lookup`;
CREATE TABLE `build_type_lookup` (
  `build_type_id` tinyint(4) NOT NULL auto_increment,
  `name` varchar(64) collate latin1_bin NOT NULL default '',
  PRIMARY KEY  (`build_type_id`),
  KEY `name` (`name`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--
-- Table structure for table `exit_status_lookup`
--

DROP TABLE IF EXISTS `exit_status_lookup`;
CREATE TABLE `exit_status_lookup` (
  `exit_status_id` tinyint(4) NOT NULL auto_increment,
  `name` varchar(64) collate latin1_bin NOT NULL default '',
  PRIMARY KEY  (`exit_status_id`),
  KEY `name` (`name`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--
-- Table structure for table `log_type_lookup`
--

DROP TABLE IF EXISTS `log_type_lookup`;
CREATE TABLE `log_type_lookup` (
  `log_type_id` tinyint(4) NOT NULL auto_increment,
  `name` varchar(64) collate latin1_bin NOT NULL default '',
  PRIMARY KEY  (`log_type_id`),
  KEY `name` (`name`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--
-- Table structure for table `opsyses`
--

DROP TABLE IF EXISTS `opsyses`;
CREATE TABLE `opsyses` (
  `opsys_id` smallint(6) NOT NULL auto_increment,
  `platform_id` smallint(6) NOT NULL default '0',
  `name` varchar(64) collate latin1_bin NOT NULL default '',
  `detect_regexp` varchar(255) collate latin1_bin default NULL,
  PRIMARY KEY  (`opsys_id`),
  KEY `platform_id` (`platform_id`),
  KEY `name` (`name`),
  KEY `detect_regexp` (`detect_regexp`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--
-- Table structure for table `platforms`
--

DROP TABLE IF EXISTS `platforms`;
CREATE TABLE `platforms` (
  `platform_id` smallint(6) NOT NULL auto_increment,
  `product_id` tinyint(4) NOT NULL default '0',
  `name` varchar(64) collate latin1_bin NOT NULL default '',
  `detect_regexp` varchar(255) collate latin1_bin default NULL,
  `iconpath` varchar(255) collate latin1_bin default NULL,
  PRIMARY KEY  (`platform_id`),
  KEY `product_id` (`product_id`),
  KEY `name` (`name`),
  KEY `detect_regexp` (`detect_regexp`),
  KEY `iconpath` (`iconpath`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--
-- Table structure for table `products`
--

DROP TABLE IF EXISTS `products`;
CREATE TABLE `products` (
  `product_id` tinyint(4) NOT NULL auto_increment,
  `name` varchar(64) collate latin1_bin NOT NULL default '',
  `iconpath` varchar(255) collate latin1_bin default NULL,
  PRIMARY KEY  (`product_id`),
  KEY `name` (`name`),
  KEY `iconpath` (`iconpath`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--
-- Table structure for table `subgroups`
--

DROP TABLE IF EXISTS `subgroups`;
CREATE TABLE `subgroups` (
  `subgroup_id` smallint(6) NOT NULL auto_increment,
  `testgroup_id` smallint(6) NOT NULL default '0',
  `name` varchar(64) collate latin1_bin NOT NULL default '',
  PRIMARY KEY  (`subgroup_id`),
  KEY `testgroup_id` (`testgroup_id`),
  KEY `name` (`name`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--
-- Table structure for table `test_format_lookup`
--

DROP TABLE IF EXISTS `test_format_lookup`;
CREATE TABLE `test_format_lookup` (
  `format_id` tinyint(4) NOT NULL auto_increment,
  `name` varchar(255) collate latin1_bin NOT NULL default '',
  PRIMARY KEY  (`format_id`),
  KEY `name` (`name`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--
-- Table structure for table `test_groups`
--

DROP TABLE IF EXISTS `test_groups`;
CREATE TABLE `test_groups` (
  `testgroup_id` smallint(6) NOT NULL auto_increment,
  `product_id` tinyint(4) NOT NULL default '0',
  `name` varchar(64) collate latin1_bin NOT NULL default '',
  `expiration_days` smallint(6) NOT NULL default '0',
  PRIMARY KEY  (`testgroup_id`),
  KEY `product_id` (`product_id`),
  KEY `name` (`name`),
  KEY `expiration_days` (`expiration_days`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--
-- Table structure for table `test_result_bugs`
--

DROP TABLE IF EXISTS `test_result_bugs`;
CREATE TABLE `test_result_bugs` (
  `test_result_id` int(11) NOT NULL auto_increment,
  `last_updated` datetime NOT NULL default '0000-00-00 00:00:00',
  `submission_time` datetime NOT NULL default '0000-00-00 00:00:00',
  `user_id` int(11) default NULL,
  `bug_id` int(11) NOT NULL default '0',
  PRIMARY KEY  (`test_result_id`,`bug_id`),
  KEY `test_result_id` (`test_result_id`),
  KEY `last_updated` (`last_updated`),
  KEY `submission_time` (`submission_time`),
  KEY `user_id` (`user_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--
-- Table structure for table `test_result_comments`
--

DROP TABLE IF EXISTS `test_result_comments`;
CREATE TABLE `test_result_comments` (
  `comment_id` int(11) NOT NULL auto_increment,
  `test_result_id` int(11) NOT NULL default '0',
  `last_updated` datetime NOT NULL default '0000-00-00 00:00:00',
  `submission_time` datetime NOT NULL default '0000-00-00 00:00:00',
  `user_id` int(11) default NULL,
  `comment` text collate latin1_bin,
  PRIMARY KEY  (`comment_id`),
  KEY `test_result_id` (`test_result_id`),
  KEY `last_updated` (`last_updated`),
  KEY `submission_time` (`submission_time`),
  KEY `user_id` (`user_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--
-- Table structure for table `test_result_logs`
--

DROP TABLE IF EXISTS `test_result_logs`;
CREATE TABLE `test_result_logs` (
  `log_id` int(11) NOT NULL auto_increment,
  `test_result_id` int(11) NOT NULL default '0',
  `last_updated` datetime NOT NULL default '0000-00-00 00:00:00',
  `submission_time` datetime NOT NULL default '0000-00-00 00:00:00',
  `log_path` varchar(255) collate latin1_bin default NULL,
  `log_type_id` tinyint(4) NOT NULL default '1',
  PRIMARY KEY  (`log_id`),
  KEY `test_result_id` (`test_result_id`),
  KEY `last_updated` (`last_updated`),
  KEY `submission_time` (`submission_time`),
  KEY `log_path` (`log_path`),
  KEY `log_type_id` (`log_type_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--
-- Table structure for table `test_result_status_lookup`
--

DROP TABLE IF EXISTS `test_result_status_lookup`;
CREATE TABLE `test_result_status_lookup` (
  `result_status_id` smallint(6) NOT NULL auto_increment,
  `name` varchar(64) collate latin1_bin NOT NULL default '',
  `style` varchar(255) collate latin1_bin NOT NULL default '',
  `class_name` varchar(16) collate latin1_bin default '',
  PRIMARY KEY  (`result_status_id`),
  KEY `name` (`name`),
  KEY `style` (`style`),
  KEY `class_name` (`class_name`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--
-- Table structure for table `test_results`
--

DROP TABLE IF EXISTS `test_results`;
CREATE TABLE `test_results` (
  `testresult_id` int(11) NOT NULL auto_increment,
  `test_id` int(11) NOT NULL default '0',
  `last_updated` datetime NOT NULL default '0000-00-00 00:00:00',
  `submission_time` datetime NOT NULL default '0000-00-00 00:00:00',
  `user_id` int(11) default NULL,
  `platform_id` smallint(6) default NULL,
  `opsys_id` smallint(6) default NULL,
  `branch_id` smallint(6) default NULL,
  `buildid` varchar(45) collate latin1_bin default NULL,
  `user_agent` varchar(255) collate latin1_bin default NULL,
  `result_id` smallint(6) default NULL,
  `build_type_id` tinyint(4) NOT NULL default '1',
  `machine_name` varchar(64) collate latin1_bin default NULL,
  `exit_status_id` tinyint(4) NOT NULL default '1',
  `duration_ms` int(11) unsigned default NULL,
  `talkback_id` int(11) unsigned default '0',
  `validity_id` tinyint(4) NOT NULL default '1',
  `vetting_status_id` tinyint(4) NOT NULL default '1',
  PRIMARY KEY  (`testresult_id`),
  KEY `test_id` (`test_id`),
  KEY `last_updated` (`last_updated`),
  KEY `submission_time` (`submission_time`),
  KEY `user_id` (`user_id`),
  KEY `platform_id` (`platform_id`),
  KEY `opsys_id` (`opsys_id`),
  KEY `branch_id` (`branch_id`),
  KEY `user_agent` (`user_agent`),
  KEY `result_id` (`result_id`),
  KEY `build_type_id` (`build_type_id`),
  KEY `machine_name` (`machine_name`),
  KEY `exit_status_id` (`exit_status_id`),
  KEY `duration_ms` (`duration_ms`),
  KEY `talkback_id` (`talkback_id`),
  KEY `validity_id` (`validity_id`),
  KEY `vetting_status_id` (`vetting_status_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--
-- Table structure for table `test_status_lookup`
--

DROP TABLE IF EXISTS `test_status_lookup`;
CREATE TABLE `test_status_lookup` (
  `test_status_id` tinyint(4) NOT NULL auto_increment,
  `name` varchar(64) collate latin1_bin NOT NULL default '',
  PRIMARY KEY  (`test_status_id`),
  KEY `name` (`name`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--
-- Table structure for table `tests`
--

DROP TABLE IF EXISTS `tests`;
CREATE TABLE `tests` (
  `test_id` int(11) NOT NULL auto_increment,
  `subgroup_id` smallint(6) NOT NULL default '0',
  `summary` varchar(255) collate latin1_bin NOT NULL default '',
  `details` text collate latin1_bin,
  `status_id` tinyint(4) NOT NULL default '0',
  `community_enabled` tinyint(1) default NULL,
  `format_id` tinyint(4) NOT NULL default '1',
  `regression_bug_id` int(11) default NULL,
  `t1` longtext collate latin1_bin,
  `t2` longtext collate latin1_bin,
  `t3` longtext collate latin1_bin,
  PRIMARY KEY  (`test_id`),
  KEY `subgroup_id` (`subgroup_id`),
  KEY `summary` (`summary`),
  KEY `status_id` (`status_id`),
  KEY `community_enabled` (`community_enabled`),
  KEY `format_id` (`format_id`),
  KEY `regression_bug_id` (`regression_bug_id`),
  KEY `t1` (`t1`(255)),
  KEY `t2` (`t2`(255)),
  KEY `t3` (`t3`(255))
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--
-- Table structure for table `users`
--

DROP TABLE IF EXISTS `users`;
CREATE TABLE `users` (
  `user_id` int(11) NOT NULL auto_increment,
  `email` varchar(255) collate latin1_bin NOT NULL default '',
  `is_trusted` tinyint(1) default NULL,
  PRIMARY KEY  (`user_id`),
  KEY `email` (`email`),
  KEY `is_trusted` (`is_trusted`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--
-- Table structure for table `validity_lookup`
--

DROP TABLE IF EXISTS `validity_lookup`;
CREATE TABLE `validity_lookup` (
  `validity_id` tinyint(4) NOT NULL auto_increment,
  `name` varchar(64) collate latin1_bin NOT NULL default '',
  PRIMARY KEY  (`validity_id`),
  KEY `name` (`name`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

--
-- Table structure for table `vetting_status_lookup`
--

DROP TABLE IF EXISTS `vetting_status_lookup`;
CREATE TABLE `vetting_status_lookup` (
  `vetting_status_id` tinyint(4) NOT NULL auto_increment,
  `name` varchar(64) collate latin1_bin NOT NULL default '',
  PRIMARY KEY  (`vetting_status_id`),
  KEY `name` (`name`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COLLATE=latin1_bin;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;

