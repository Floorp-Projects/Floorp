-- MySQL dump 9.08
--
-- Host: mecha    Database: downloadstats
---------------------------------------------------------
-- Server version	4.0.14-standard-log

--
-- Table structure for table 'entries'
--

CREATE TABLE entries (
  id int(11) NOT NULL default '0',
  protocol varchar(4) default NULL,
  protocol_version varchar(5) default NULL,
  client varchar(50) default NULL,
  date_time datetime default NULL,
  method varchar(4) default NULL,
  file_id int(11) default NULL,
  status char(3) default NULL,
  bytes int(11) default NULL,
  site_id tinyint(4) default NULL,
  log_id int(11) default NULL,
  PRIMARY KEY  (id),
  KEY file_id (file_id,date_time,bytes)
) TYPE=MyISAM;

--
-- Table structure for table 'files'
--

CREATE TABLE files (
  id int(11) NOT NULL default '0',
  path varchar(150) default NULL,
  name varchar(100) default NULL,
  PRIMARY KEY  (id),
  UNIQUE KEY path (path,name)
) TYPE=MyISAM;

--
-- Table structure for table 'logs'
--

CREATE TABLE logs (
  id int(11) NOT NULL default '0',
  path varchar(100) default NULL,
  name varchar(50) default NULL,
  site_id tinyint(4) default NULL,
  status varchar(10) default NULL,
  PRIMARY KEY  (id)
) TYPE=MyISAM;

--
-- Table structure for table 'sites'
--

CREATE TABLE sites (
  id int(11) NOT NULL default '0',
  abbr varchar(15) default NULL,
  PRIMARY KEY  (id),
  UNIQUE KEY abbr (abbr)
) TYPE=MyISAM;

