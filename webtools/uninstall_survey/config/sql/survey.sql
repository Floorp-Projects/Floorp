-- MySQL dump 10.9
--
-- Host: localhost    Database: survey
-- ------------------------------------------------------
-- Server version	4.1.12

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `applications`
--

DROP TABLE IF EXISTS `applications`;
CREATE TABLE `applications` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `name` varchar(255) default NULL,
  `version` varchar(255) default NULL,
  `visible` int(1) default '0',
  PRIMARY KEY  (`id`),
  KEY `app_nam_idx` (`name`),
  KEY `app_ver_idx` (`version`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

--
-- Table structure for table `applications_intentions`
--

DROP TABLE IF EXISTS `applications_intentions`;
CREATE TABLE `applications_intentions` (
  `application_id` int(10) unsigned default NULL,
  `intention_id` int(10) unsigned default NULL,
  KEY `app_int_idx` (`application_id`,`intention_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

--
-- Table structure for table `applications_issues`
--

DROP TABLE IF EXISTS `applications_issues`;
CREATE TABLE `applications_issues` (
  `application_id` int(10) unsigned default NULL,
  `issue_id` int(10) unsigned default NULL,
  KEY `app_iss_idx` (`application_id`,`issue_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

--
-- Table structure for table `intentions`
--

DROP TABLE IF EXISTS `intentions`;
CREATE TABLE `intentions` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `description` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  `pos` int(10) unsigned NOT NULL default '0',
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

--
-- Table structure for table `issues`
--

DROP TABLE IF EXISTS `issues`;
CREATE TABLE `issues` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `description` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  `pos` int(10) unsigned NOT NULL default '0',
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

--
-- Table structure for table `issues_results`
--

DROP TABLE IF EXISTS `issues_results`;
CREATE TABLE `issues_results` (
  `result_id` bigint(20) unsigned NOT NULL default '0',
  `issue_id` int(10) unsigned NOT NULL default '0',
  `other` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  PRIMARY KEY  (`result_id`,`issue_id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

--
-- Table structure for table `results`
--

DROP TABLE IF EXISTS `results`;
CREATE TABLE `results` (
  `id` bigint(20) unsigned NOT NULL auto_increment,
  `application_id` int(10) unsigned default NULL,
  `intention_id` int(10) unsigned default NULL,
  `intention_text` varchar(255) collate utf8_unicode_ci default NULL,
  `useragent` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  `http_user_agent` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  `comments` text collate utf8_unicode_ci NOT NULL,
  `created` datetime default NULL,
  `modified` datetime default NULL,
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

