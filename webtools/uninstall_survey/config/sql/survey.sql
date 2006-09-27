-- MySQL dump 10.9
--
-- Host: localhost    Database: uninstall_survey
-- ------------------------------------------------------
-- Server version	4.1.20

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
-- Dumping data for table `applications`
--


/*!40000 ALTER TABLE `applications` DISABLE KEYS */;
LOCK TABLES `applications` WRITE;
INSERT INTO `applications` VALUES (4,'Mozilla Firefox','1.5.0.1',1),(3,'Mozilla Firefox','1.5',1),(5,'Mozilla Firefox','1.5.0.2',1),(6,'Mozilla Firefox','1.5.0.3',1),(7,'Mozilla Firefox','1.5.0.4',1),(8,'Mozilla Thunderbird','1.5',1),(9,'Mozilla Thunderbird','1.6a1',1),(10,'Mozilla Thunderbird','2.0a1',1),(11,'Mozilla Thunderbird','3.0a1',1),(12,'Mozilla Thunderbird','1.5.0.4',0),(13,'Bon Echo','2.0a2',0),(14,'Minefield','3.0a1',0),(15,'Mozilla Thunderbird','1.5.0.2',0),(16,NULL,NULL,0),(17,NULL,NULL,0),(18,'Bon Echo','2.0a3',0),(19,'Deer Park Alpha 2','1.6a1',0),(20,'Deer Park Alpha 2','3.0a1',0),(21,'lolifox','0.2.1',0),(22,'Bon Echo','2.0a1',0),(23,'lolifox','0.2.2',0),(24,'Mozilla Firefox','3.0a1',0),(25,'Mozilla Firefox','1.5.0.3.1',0),(26,NULL,NULL,0),(27,'Mozilla Firefox','2.0a3',0),(28,'lolifox','0.2.3',0),(29,NULL,NULL,0),(30,'Mozzilla Firefox','1.5.0.4',0),(31,NULL,NULL,0),(32,NULL,NULL,0),(33,NULL,NULL,0),(34,'Firefox Community Edition','1.5',0),(35,NULL,NULL,0),(36,'Mozilla Firefox','1.5.0.4.1',0),(37,'Mozilla Sunbird','${AppVersion}',0),(38,'Mozilla Firefox','2.0a1',0),(39,NULL,NULL,0),(40,NULL,NULL,0),(41,NULL,NULL,0),(42,NULL,NULL,0),(43,'lolifox','0.2',0),(44,NULL,NULL,0),(45,'Mozilla Firefox','Bon Echo',0),(46,'Unison','0.0001a',0),(47,'Mekhala','1.5',0),(48,NULL,NULL,0),(49,NULL,NULL,0),(50,NULL,NULL,0),(51,NULL,NULL,0),(52,NULL,NULL,0),(53,NULL,NULL,0),(54,'Mozilla Firefox','${AppVersion}',0),(55,'Mozilla Firefox','2.0b1',0),(56,'Bon Echo','2.0b1',0),(57,NULL,NULL,0),(58,'Thunderbird Community Edition','1.5',0),(59,'Mozzila Firefox','1.5.0.4',0),(60,'Mozilla Firefox','AppVersion',0),(61,NULL,NULL,0),(62,NULL,NULL,0),(63,'Mozilla Firefox','0.2.4',0),(64,NULL,NULL,0),(65,NULL,NULL,0),(66,'Bon Echo','1.5',0),(67,NULL,NULL,0),(68,NULL,NULL,0),(69,'mozilla firefox','20b1',0),(70,NULL,NULL,0),(71,NULL,NULL,0),(72,'mizilla Firefox','1.5.0.4',0),(73,'Mozilla Thunderbird','1.5.04',0),(74,'Mozilla Thunderbird','15',0),(75,'Mozilla Forefox','1.5',0),(76,'Mozilla Firefox','1.5.0.5',0),(77,NULL,NULL,0),(78,'Mozilla Thunderbird','1.5.0.5',0),(79,'Mozilla Firefox','2.0a',0),(80,'Mozilla Firefox','2.0b',0),(81,NULL,NULL,0),(82,NULL,NULL,0),(83,'Mozilla Firefox','1.7',0),(84,'Mozilla Thunderbird','${AppVersion}',0),(85,'Mozilla Firefox','1.5.0.6',0),(86,NULL,NULL,0),(87,NULL,NULL,0),(88,NULL,NULL,0),(89,NULL,NULL,0),(90,'Thunderbird','1.5',0),(91,'Mozilla0Firefox','2.0b1',0),(92,NULL,NULL,0),(93,NULL,NULL,0),(94,NULL,NULL,0),(95,'Mozilla Firefox','@MOZ_APP_VERSION@',0),(96,'Mozilla Firefox','2.0b2',0),(97,NULL,NULL,0),(98,NULL,NULL,0),(99,'1.5 Firefox','1.5',0),(100,NULL,NULL,0),(101,'Mozilla 20Firefox','1.5',0),(102,'Bon Echo','2.0b2',0),(103,NULL,NULL,0),(104,NULL,NULL,0),(105,'Mozilla Firefox','0.2.5.dev-r26',0),(106,NULL,NULL,0),(107,NULL,NULL,0),(108,NULL,NULL,0),(109,NULL,NULL,0),(110,NULL,NULL,0),(111,NULL,NULL,0),(112,'Mozilla Sunbird','0.3',0),(113,'Mozilla Sunbird','0.3a2',0),(114,NULL,NULL,0),(115,NULL,NULL,0),(116,NULL,NULL,0),(117,NULL,NULL,0),(118,NULL,NULL,0),(119,NULL,NULL,0),(120,NULL,NULL,0),(121,NULL,NULL,0),(122,NULL,NULL,0),(123,'mozilla Firefox','1.5.06',0),(124,NULL,NULL,0),(125,NULL,NULL,0),(126,NULL,NULL,0),(127,NULL,NULL,0),(128,'Mozilla Thunderbird','2.0b1pre',0),(129,'Mozilla Firefox','2.0',0),(130,'Mozilla Firefox','1.5.0.7',0),(131,'Mozilla Thunderbird','1.5.0.7',0),(132,'Angelsoft','1.1',0),(133,NULL,NULL,0),(134,NULL,NULL,0),(135,NULL,NULL,0),(136,NULL,NULL,0),(137,NULL,NULL,0),(138,NULL,NULL,0),(139,NULL,NULL,0),(140,'Mozilla Firefox','1.5.0.7.1',0),(141,'Mozilla Firefox','1.5.0.8pre',0),(142,NULL,NULL,0),(143,NULL,NULL,0),(144,NULL,NULL,0),(145,NULL,NULL,0),(146,'Mozilla Firefox','0.2.5.dev-r27',0),(147,'Mozilla Firefox','1.5.0.7 (pt-BR;',0),(148,'moxilla firefox','1.5',0);
UNLOCK TABLES;
/*!40000 ALTER TABLE `applications` ENABLE KEYS */;

--
-- Table structure for table `applications_collections`
--

DROP TABLE IF EXISTS `applications_collections`;
CREATE TABLE `applications_collections` (
  `application_id` int(10) unsigned NOT NULL default '0',
  `collection_id` int(10) unsigned NOT NULL default '0',
  PRIMARY KEY  (`application_id`,`collection_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

--
-- Dumping data for table `applications_collections`
--


/*!40000 ALTER TABLE `applications_collections` DISABLE KEYS */;
LOCK TABLES `applications_collections` WRITE;
INSERT INTO `applications_collections` VALUES (3,1),(3,2),(4,1),(4,2),(5,1),(5,2),(6,1),(6,2),(7,1),(7,2),(8,3),(8,4),(9,3),(9,4),(10,3),(10,4),(11,3),(11,4),(12,3),(12,4),(15,3),(15,4),(58,3),(58,4),(59,1),(59,2),(60,1),(60,2),(63,1),(63,2),(69,1),(69,2),(72,1),(72,2),(73,3),(73,4),(74,3),(74,4),(76,1),(76,2),(78,3),(78,4),(79,1),(79,2),(80,1),(80,2),(83,1),(83,2),(84,3),(84,4),(85,1),(85,2),(90,3),(90,4),(91,1),(91,2),(95,1),(95,2),(96,1),(96,2),(99,1),(99,2),(101,1),(101,2),(105,1),(105,2),(123,1),(123,2),(128,3),(128,4),(129,1),(129,2),(130,1),(130,2),(131,3),(131,4),(140,1),(140,2),(141,1),(141,2),(146,1),(146,2),(147,1),(147,2),(148,1),(148,2);
UNLOCK TABLES;
/*!40000 ALTER TABLE `applications_collections` ENABLE KEYS */;

--
-- Table structure for table `choices`
--

DROP TABLE IF EXISTS `choices`;
CREATE TABLE `choices` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `description` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  `pos` int(10) unsigned NOT NULL default '0',
  `type` enum('issue','intention') collate utf8_unicode_ci default NULL,
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;

--
-- Dumping data for table `choices`
--


/*!40000 ALTER TABLE `choices` DISABLE KEYS */;
LOCK TABLES `choices` WRITE;
INSERT INTO `choices` VALUES (1,'Some web pages wouldn\'t work',200,'issue'),(2,'Plugin compatibility (Flash, Adobe Acrobat, Windows Media Player, etc.)',0,'issue'),(3,'Performance (load delays, memory usage)',0,'issue'),(4,'Hard to use/confusing (menus, display, etc.)',0,'issue'),(5,'Missing features',0,'issue'),(6,'Security',0,'issue'),(7,'Some features didn\'t work',0,'issue'),(8,'Printing',0,'issue'),(9,'Just temporary, I\'m planning to install Firefox again soon',201,'issue'),(10,'Just temporary, I\'m planning to install Thunderbird again soon',50,'issue'),(11,'Lack of calendaring support',40,'issue'),(12,'Performance (long time to start up, etc.)',30,'issue'),(13,'Unable to read my mail',20,'issue'),(14,'Missing features',10,'issue'),(15,'Other',2000,'issue'),(16,'To fully replace my previous web browser',0,'intention'),(17,'To use in addition to another web browser',0,'intention'),(18,'I just wanted to try it out',0,'intention'),(19,'To manage e-mail for an ISP account',1500,'intention'),(20,'To manage e-mail in a corporate environment',105,'intention'),(21,'To manage e-mail from a webmail account (Yahoo! Mail, AOL, Gmail, Hotmail, other)',90,'intention'),(22,'I just wanted to try it out',2,'intention'),(23,'Other',2000,'intention');
UNLOCK TABLES;
/*!40000 ALTER TABLE `choices` ENABLE KEYS */;

--
-- Table structure for table `choices_collections`
--

DROP TABLE IF EXISTS `choices_collections`;
CREATE TABLE `choices_collections` (
  `choice_id` int(10) unsigned NOT NULL default '0',
  `collection_id` int(10) unsigned NOT NULL default '0',
  PRIMARY KEY  (`choice_id`,`collection_id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

--
-- Dumping data for table `choices_collections`
--


/*!40000 ALTER TABLE `choices_collections` DISABLE KEYS */;
LOCK TABLES `choices_collections` WRITE;
INSERT INTO `choices_collections` VALUES (1,1),(2,1),(3,1),(4,1),(5,1),(6,1),(7,1),(8,1),(9,1),(10,3),(11,3),(12,3),(13,3),(14,3),(15,1),(15,3),(16,2),(17,2),(18,2),(18,4),(19,4),(20,4),(21,4),(23,2),(23,4);
UNLOCK TABLES;
/*!40000 ALTER TABLE `choices_collections` ENABLE KEYS */;

--
-- Table structure for table `choices_results`
--

DROP TABLE IF EXISTS `choices_results`;
CREATE TABLE `choices_results` (
  `result_id` bigint(20) unsigned NOT NULL default '0',
  `choice_id` int(10) unsigned NOT NULL default '0',
  `other` varchar(255) collate utf8_unicode_ci NOT NULL default '',
  PRIMARY KEY  (`result_id`,`choice_id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE=utf8_unicode_ci;


--
-- Table structure for table `collections`
--

DROP TABLE IF EXISTS `collections`;
CREATE TABLE `collections` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `description` varchar(255) NOT NULL default '',
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1;

--
-- Dumping data for table `collections`
--


/*!40000 ALTER TABLE `collections` DISABLE KEYS */;
LOCK TABLES `collections` WRITE;
INSERT INTO `collections` VALUES (1,'Original Issues - fx','2006-06-26 15:02:24','0000-00-00 00:00:00'),(2,'Original Intentions - fx','2006-06-26 15:02:41','0000-00-00 00:00:00'),(3,'Original Issues - tb','2006-06-26 15:03:39','0000-00-00 00:00:00'),(4,'Original Intentions - tb','2006-06-26 15:03:59','0000-00-00 00:00:00');
UNLOCK TABLES;
/*!40000 ALTER TABLE `collections` ENABLE KEYS */;

--
-- Table structure for table `results`
--

DROP TABLE IF EXISTS `results`;
CREATE TABLE `results` (
  `id` bigint(20) unsigned NOT NULL auto_increment,
  `application_id` int(10) unsigned default NULL,
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

