# phpMyAdmin MySQL-Dump
# version 2.2.1
# http://phpwizard.net/phpMyAdmin/
# http://phpmyadmin.sourceforge.net/ (download page)
#
# Host: bonsai2
# Generation Time: Feb 12, 2002 at 09:31 PM
# Server version: 3.23.46
# PHP Version: 4.0.3pl1
# Database : `bonsai`
# --------------------------------------------------------

#
# Table structure for table `accessconfig`
#

CREATE TABLE `accessconfig` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `time` timestamp(14) NOT NULL,
  `cvsroot_id` int(10) unsigned NOT NULL default '0',
  `rev` varchar(128) NOT NULL default '',
  `value` mediumtext NOT NULL,
  PRIMARY KEY  (`id`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `branch`
#

CREATE TABLE `branch` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `value` varchar(64) binary NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `value` (`value`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `change`
#

CREATE TABLE `change` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `checkin_id` int(10) unsigned NOT NULL default '0',
  `file_id` int(10) unsigned NOT NULL default '0',
  `oldrev` varchar(128) NOT NULL default '',
  `newrev` varchar(128) NOT NULL default '',
  `branch_id` int(10) unsigned NOT NULL default '0',
  PRIMARY KEY  (`id`),
  KEY `checkin_id` (`checkin_id`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `checkin`
#

CREATE TABLE `checkin` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `user_id` int(10) unsigned NOT NULL default '0',
  `time` int(10) unsigned NOT NULL default '0',
  `directory_id` int(10) unsigned NOT NULL default '0',
  `log_id` int(10) unsigned NOT NULL default '0',
  `cvsroot_id` int(10) unsigned NOT NULL default '0',
  PRIMARY KEY  (`id`),
  KEY `time` (`time`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `cvsroot`
#

CREATE TABLE `cvsroot` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `value` varchar(128) binary NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `value` (`value`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `cvsroot_branch_map_eol`
#

CREATE TABLE `cvsroot_branch_map_eol` (
  `cvsroot_id` int(10) unsigned NOT NULL default '0',
  `branch_id` int(10) unsigned NOT NULL default '0',
  `timestamp` timestamp(14) NOT NULL,
  PRIMARY KEY  (`cvsroot_id`,`branch_id`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `directory`
#

CREATE TABLE `directory` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `value` varchar(128) binary NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `value` (`value`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `exec_log`
#

CREATE TABLE `exec_log` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `time` int(10) unsigned NOT NULL default '0',
  `command` text NOT NULL,
  `stdout` mediumtext,
  `stderr` mediumtext,
  `exit_value` smallint(5) unsigned default '0',
  `signal_num` tinyint(3) unsigned default '0',
  `dumped_core` tinyint(3) unsigned default '0',
  PRIMARY KEY  (`id`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `expanded_accessconfig`
#

CREATE TABLE `expanded_accessconfig` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `time` timestamp(14) NOT NULL,
  `value` mediumtext NOT NULL,
  PRIMARY KEY  (`id`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `expanded_mirrorconfig`
#

CREATE TABLE `expanded_mirrorconfig` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `time` timestamp(14) NOT NULL,
  `value` mediumtext NOT NULL,
  PRIMARY KEY  (`id`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `file`
#

CREATE TABLE `file` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `value` varchar(128) binary NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `value` (`value`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `group`
#

CREATE TABLE `group` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `value` varchar(64) binary NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `value` (`value`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `group_user_map`
#

CREATE TABLE `group_user_map` (
  `group_id` int(10) unsigned NOT NULL default '0',
  `user_id` int(10) unsigned NOT NULL default '0'
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `log`
#

CREATE TABLE `log` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `value` text NOT NULL,
  PRIMARY KEY  (`id`),
  KEY `value` (`value`(25))
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `loginfo_performance`
#

CREATE TABLE `loginfo_performance` (
  `checkin_id` int(10) unsigned NOT NULL default '0',
  `time` float unsigned NOT NULL default '0',
  PRIMARY KEY  (`checkin_id`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `mh_command`
#

CREATE TABLE `mh_command` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `value` varchar(32) NOT NULL default '',
  PRIMARY KEY  (`id`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `mh_hostname`
#

CREATE TABLE `mh_hostname` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `value` varchar(64) NOT NULL default '',
  PRIMARY KEY  (`id`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `mh_runtime_info`
#

CREATE TABLE `mh_runtime_info` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `time` int(10) unsigned NOT NULL default '0',
  `last_update` timestamp(14) NOT NULL,
  `mh_hostname_id` int(10) unsigned NOT NULL default '0',
  `mh_command_id` int(10) unsigned NOT NULL default '0',
  `mh_command_response` int(10) unsigned NOT NULL default '0',
  `mirror_delay` smallint(5) unsigned NOT NULL default '0',
  `min_scan_time` smallint(5) unsigned NOT NULL default '0',
  `throttle_time` smallint(5) unsigned NOT NULL default '0',
  `max_addcheckins` smallint(5) unsigned NOT NULL default '0',
  PRIMARY KEY  (`id`),
  KEY `id` (`id`,`time`,`mh_hostname_id`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `mirror`
#

CREATE TABLE `mirror` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `checkin_id` int(10) unsigned NOT NULL default '0',
  `branch_id` int(10) unsigned NOT NULL default '0',
  `cvsroot_id` int(10) unsigned NOT NULL default '0',
  `offset_id` int(10) unsigned NOT NULL default '0',
  `status_id` int(10) unsigned NOT NULL default '0',
  PRIMARY KEY  (`id`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `mirror_change_exec_map`
#

CREATE TABLE `mirror_change_exec_map` (
  `mirror_id` int(10) unsigned NOT NULL default '0',
  `change_id` int(10) unsigned NOT NULL default '0',
  `exec_log_id` int(10) unsigned NOT NULL default '0'
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `mirror_change_map`
#

CREATE TABLE `mirror_change_map` (
  `mirror_id` int(10) unsigned NOT NULL default '0',
  `change_id` int(10) unsigned NOT NULL default '0',
  `type_id` int(10) unsigned NOT NULL default '0',
  `status_id` int(10) unsigned NOT NULL default '0',
  KEY `mirror_id` (`mirror_id`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `mirrorconfig`
#

CREATE TABLE `mirrorconfig` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `time` timestamp(14) NOT NULL,
  `cvsroot_id` int(10) unsigned NOT NULL default '0',
  `rev` varchar(128) NOT NULL default '',
  `value` mediumtext NOT NULL,
  PRIMARY KEY  (`id`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `modules`
#

CREATE TABLE `modules` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `time` timestamp(14) NOT NULL,
  `cvsroot_id` int(10) unsigned NOT NULL default '0',
  `rev` varchar(128) NOT NULL default '',
  `value` mediumtext NOT NULL,
  PRIMARY KEY  (`id`),
  KEY `cvsroot_id` (`cvsroot_id`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `offset`
#

CREATE TABLE `offset` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `value` varchar(128) binary NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `value` (`value`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `status`
#

CREATE TABLE `status` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `value` varchar(32) binary NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `value` (`value`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `temp_commitinfo`
#

CREATE TABLE `temp_commitinfo` (
  `cwd` varchar(255) NOT NULL default '',
  `user_id` int(10) unsigned NOT NULL default '0',
  `time` int(10) unsigned NOT NULL default '0',
  `directory_id` int(10) unsigned NOT NULL default '0',
  `cvsroot_id` int(10) unsigned NOT NULL default '0',
  `files` text NOT NULL,
  `status` varchar(32) NOT NULL default '',
  PRIMARY KEY  (`cwd`,`user_id`,`time`,`directory_id`,`cvsroot_id`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `type`
#

CREATE TABLE `type` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `value` varchar(16) binary NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `value` (`value`)
) TYPE=MyISAM;
# --------------------------------------------------------

#
# Table structure for table `user`
#

CREATE TABLE `user` (
  `id` int(10) unsigned NOT NULL auto_increment,
  `value` varchar(32) NOT NULL default '',
  PRIMARY KEY  (`id`),
  UNIQUE KEY `value` (`value`)
) TYPE=MyISAM;

