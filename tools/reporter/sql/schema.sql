-- Database: `reporter`
-- 

-- --------------------------------------------------------

-- 
-- Table structure for table `host`
-- 

CREATE TABLE "host" (
  "host_id" varchar(32) collate utf8_unicode_ci NOT NULL default '',
  "host_hostname" varchar(255) collate utf8_unicode_ci NOT NULL default '',
  "host_date_added" datetime NOT NULL default '0000-00-00 00:00:00',
  "host_user_added" varchar(60) collate utf8_unicode_ci NOT NULL default '',
  PRIMARY KEY  ("host_id"),
  KEY "host_hostname" ("host_hostname")
);

-- --------------------------------------------------------

-- 
-- Table structure for table `product`
-- 

CREATE TABLE "product" (
  "product_id" int(11) NOT NULL auto_increment,
  "product_value" varchar(150) collate utf8_unicode_ci NOT NULL default '',
  "product_family" varchar(12) collate utf8_unicode_ci NOT NULL default '',
  "product_description" varchar(150) collate utf8_unicode_ci NOT NULL default '',
  PRIMARY KEY  ("product_id"),
  KEY "product_family" ("product_family"),
  KEY "product_value" ("product_value")
) AUTO_INCREMENT=3 ;

-- --------------------------------------------------------

-- 
-- Table structure for table `report`
-- 

CREATE TABLE "report" (
  "report_id" varchar(17) collate utf8_unicode_ci NOT NULL default '',
  "report_url" varchar(255) collate utf8_unicode_ci NOT NULL default '',
  "report_host_id" varchar(32) collate utf8_unicode_ci NOT NULL default '',
  "report_problem_type" varchar(5) collate utf8_unicode_ci NOT NULL default '0',
  "report_description" text collate utf8_unicode_ci NOT NULL,
  "report_behind_login" int(11) NOT NULL default '0',
  "report_useragent" varchar(255) collate utf8_unicode_ci NOT NULL default '',
  "report_platform" varchar(20) collate utf8_unicode_ci NOT NULL default '',
  "report_oscpu" varchar(100) collate utf8_unicode_ci NOT NULL default '',
  "report_language" varchar(7) collate utf8_unicode_ci NOT NULL default '',
  "report_gecko" varchar(8) collate utf8_unicode_ci NOT NULL default '',
  "report_buildconfig" tinytext collate utf8_unicode_ci NOT NULL,
  "report_product" varchar(100) collate utf8_unicode_ci NOT NULL default '',
  "report_email" varchar(255) collate utf8_unicode_ci NOT NULL default '',
  "report_ip" varchar(15) collate utf8_unicode_ci NOT NULL default '',
  "report_file_date" datetime NOT NULL default '0000-00-00 00:00:00',
  "report_sysid" varchar(10) collate utf8_unicode_ci NOT NULL default '',
  PRIMARY KEY  ("report_id"),
  KEY "report_host_id" ("report_host_id")
);

-- --------------------------------------------------------

-- 
-- Table structure for table `screenshot`
-- 

CREATE TABLE "screenshot" (
  "screenshot_report_id" varchar(17) collate utf8_unicode_ci NOT NULL default '',
  "screenshot_data" longblob NOT NULL,
  "screenshot_format" varchar(14) collate utf8_unicode_ci NOT NULL default 'png',
  PRIMARY KEY  ("screenshot_report_id")
);

-- --------------------------------------------------------

-- 
-- Table structure for table `sysid`
-- 

CREATE TABLE "sysid" (
  "sysid_id" varchar(10) collate utf8_unicode_ci NOT NULL default '',
  "sysid_created" datetime NOT NULL default '0000-00-00 00:00:00',
  "sysid_created_ip" varchar(15) collate utf8_unicode_ci NOT NULL default '',
  "sysid_language" varchar(7) collate utf8_unicode_ci NOT NULL default '',
  PRIMARY KEY  ("sysid_id")
);

-- --------------------------------------------------------

-- 
-- Table structure for table `user`
--

CREATE TABLE "user" (
  "user_id" int(11) NOT NULL auto_increment,
  "user_username" varchar(50) collate utf8_unicode_ci NOT NULL default '',
  "user_password" varchar(16) collate utf8_unicode_ci NOT NULL default '',
  "user_realname" varchar(40) collate utf8_unicode_ci NOT NULL default '',
  "user_email" varchar(255) collate utf8_unicode_ci NOT NULL default '',
  "user_added_by" tinytext collate utf8_unicode_ci NOT NULL,
  "user_added_datetime" datetime NOT NULL default '0000-00-00 00:00:00',
  "user_last_login" datetime NOT NULL default '0000-00-00 00:00:00',
  "user_last_ip_address" varchar(16) collate utf8_unicode_ci NOT NULL default '',
  "user_status" int(11) NOT NULL default '0',
  PRIMARY KEY  ("user_id"),
  KEY "user_username" ("user_username")
) AUTO_INCREMENT=2 ;
