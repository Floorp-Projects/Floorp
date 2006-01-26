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
-- Table structure for table `intend`
--

DROP TABLE IF EXISTS `intend`;
CREATE TABLE `intend` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `description` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  `pos` int(10) unsigned NOT NULL default '0',
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

--
-- Dumping data for table `intend`
--


/*!40000 ALTER TABLE `intend` DISABLE KEYS */;
LOCK TABLES `intend` WRITE;
INSERT INTO `intend` VALUES (1,'To fully replace my previous web browser',0),(2,'To use in addition to another web browser',0),(3,'I just wanted to try it out',0),(4,'Other (please specify)',0);
UNLOCK TABLES;
/*!40000 ALTER TABLE `intend` ENABLE KEYS */;

--
-- Table structure for table `issue`
--

DROP TABLE IF EXISTS `issue`;
CREATE TABLE `issue` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `description` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  `other` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  `pos` int(10) unsigned NOT NULL default '0',
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

--
-- Dumping data for table `issue`
--


/*!40000 ALTER TABLE `issue` DISABLE KEYS */;
LOCK TABLES `issue` WRITE;
INSERT INTO `issue` VALUES (1,'Specific websites or content on websites didn\'t display properly','',0),(2,'Plugin compatibility (Flash, Adobe Acrobat, Windows Media Player, etc.)','',0),(3,'Performance (load delays, memory usage)','',0),(4,'Ease of use (menus, display, etc.)','',0),(5,'Missing features','',0),(6,'Security','',0),(7,'Couldn\'t get features to work','',0),(8,'Printing','',0);
UNLOCK TABLES;
/*!40000 ALTER TABLE `issue` ENABLE KEYS */;

--
-- Table structure for table `issue_result_map`
--

DROP TABLE IF EXISTS `issue_result_map`;
CREATE TABLE `issue_result_map` (
  `result_id` bigint(20) unsigned NOT NULL default '0',
  `issue_id` int(10) unsigned NOT NULL default '0',
  PRIMARY KEY  (`result_id`,`issue_id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

--
-- Dumping data for table `issue_result_map`
--


/*!40000 ALTER TABLE `issue_result_map` DISABLE KEYS */;
LOCK TABLES `issue_result_map` WRITE;
UNLOCK TABLES;
/*!40000 ALTER TABLE `issue_result_map` ENABLE KEYS */;

--
-- Table structure for table `result`
--

DROP TABLE IF EXISTS `result`;
CREATE TABLE `result` (
  `id` bigint(20) unsigned NOT NULL auto_increment,
  `intend_id` int(10) unsigned NOT NULL default '0',
  `intend_text` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  `app` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  `locale` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  `comments` text collate utf8_unicode_ci NOT NULL,
  `date_submitted` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

--
-- Dumping data for table `result`
--


/*!40000 ALTER TABLE `result` DISABLE KEYS */;
LOCK TABLES `result` WRITE;
UNLOCK TABLES;
/*!40000 ALTER TABLE `result` ENABLE KEYS */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

