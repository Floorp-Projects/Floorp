-- phpMyAdmin SQL Dump
-- version 2.7.0-pl2
-- http://www.phpmyadmin.net
-- 
-- Host: localhost
-- Generation Time: Aug 29, 2006 at 02:54 PM
-- Server version: 5.0.19
-- PHP Version: 4.4.2
-- 
-- Database: `remora`
-- 

-- --------------------------------------------------------

-- 
-- Table structure for table `addonevents`
-- 

CREATE TABLE `addonevents` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `addon_id` int(11) unsigned NOT NULL,
  `user_id` int(11) unsigned NOT NULL,
  `added` varchar(255) NOT NULL,
  `removed` varchar(255) NOT NULL,
  `notes` text,
  `created` datetime NOT NULL,
  PRIMARY KEY  (`id`),
  KEY `addon_id` (`addon_id`),
  KEY `user_id` (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- 
-- Dumping data for table `addonevents`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `addons`
-- 

CREATE TABLE `addons` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `guid` varchar(255) NOT NULL default '',
  `name` varchar(255) NOT NULL default '',
  `addontype_id` int(11) unsigned NOT NULL default '0',
  `icon` varchar(255) NOT NULL default '',
  `homepage` varchar(255) NOT NULL default '',
  `description` text NOT NULL,
  `summary` text NOT NULL,
  `averagerating` varchar(255) default NULL,
  `weeklydownloads` int(11) unsigned NOT NULL default '0',
  `totaldownloads` int(11) unsigned NOT NULL default '0',
  `developercomments` text,
  `inactive` tinyint(1) unsigned NOT NULL default '0',
  `prerelease` tinyint(1) unsigned NOT NULL default '0',
  `adminreview` tinyint(1) unsigned NOT NULL default '0',
  `sitespecific` tinyint(1) unsigned NOT NULL default '0',
  `externalsoftware` tinyint(1) unsigned NOT NULL default '0',
  `approvalnotes` text,
  `eula` text,
  `privacypolicy` text,
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`),
  KEY `addontype_id` (`addontype_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- 
-- Dumping data for table `addons`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `addons_langs`
-- 

CREATE TABLE `addons_langs` (
  `addon_id` int(11) unsigned NOT NULL,
  `lang_id` int(11) unsigned NOT NULL,
  PRIMARY KEY  (`addon_id`,`lang_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- 
-- Dumping data for table `addons_langs`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `addons_tags`
-- 

CREATE TABLE `addons_tags` (
  `addon_id` int(11) unsigned NOT NULL default '0',
  `tag_id` int(11) unsigned NOT NULL default '0',
  PRIMARY KEY  (`addon_id`,`tag_id`),
  KEY `tag_id` (`tag_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- 
-- Dumping data for table `addons_tags`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `addons_users`
-- 

CREATE TABLE `addons_users` (
  `addon_id` int(11) unsigned NOT NULL default '0',
  `user_id` int(11) unsigned NOT NULL default '0',
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`addon_id`,`user_id`),
  KEY `user_id` (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- 
-- Dumping data for table `addons_users`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `addontypes`
-- 

CREATE TABLE `addontypes` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `name` varchar(255) NOT NULL,
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- 
-- Dumping data for table `addontypes`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `applications`
-- 

CREATE TABLE `applications` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `guid` varchar(255) NOT NULL,
  `name` varchar(255) NOT NULL,
  `shortname` varchar(255) NOT NULL,
  `supported` tinyint(1) NOT NULL default '0',
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- 
-- Dumping data for table `applications`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `applications_versions`
-- 

CREATE TABLE `applications_versions` (
  `application_id` int(11) unsigned NOT NULL default '0',
  `version_id` int(11) unsigned NOT NULL default '0',
  `min` varchar(255) NOT NULL,
  `max` varchar(255) NOT NULL,
  `public` tinyint(1) unsigned NOT NULL default '0',
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`application_id`,`version_id`),
  KEY `version_id` (`version_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- 
-- Dumping data for table `applications_versions`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `approvals`
-- 

CREATE TABLE `approvals` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `user_id` int(11) unsigned NOT NULL default '0',
  `action` varchar(255) NOT NULL,
  `file_id` int(11) unsigned NOT NULL default '0',
  `comments` text NOT NULL,
  PRIMARY KEY  (`id`),
  KEY `user_id` (`user_id`),
  KEY `file_id` (`file_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- 
-- Dumping data for table `approvals`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `appversions`
-- 

CREATE TABLE `appversions` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `application_id` int(11) unsigned NOT NULL default '0',
  `version` varchar(255) NOT NULL default '',
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`),
  KEY `application_id` (`application_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- 
-- Dumping data for table `appversions`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `blapps`
-- 

CREATE TABLE `blapps` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `blitem_id` int(11) unsigned NOT NULL default '0',
  `guid` int(11) default NULL,
  `min` varchar(255) default NULL,
  `max` varchar(255) default NULL,
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`),
  KEY `blitem_id` (`blitem_id`),
  KEY `guid` (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- 
-- Dumping data for table `blapps`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `blitems`
-- 

CREATE TABLE `blitems` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `guid` varchar(255) NOT NULL,
  `min` varchar(255) default NULL,
  `max` varchar(255) default NULL,
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- 
-- Dumping data for table `blitems`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `cake_sessions`
-- 

CREATE TABLE `cake_sessions` (
  `id` varchar(255) NOT NULL default '',
  `data` text,
  `expires` int(11) default NULL,
  PRIMARY KEY  (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- 
-- Dumping data for table `cake_sessions`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `downloads`
-- 

CREATE TABLE `downloads` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `file_id` int(11) unsigned default NULL,
  `userip` varchar(255) NOT NULL default '',
  `useragent` varchar(255) NOT NULL default '',
  `counted` tinyint(1) unsigned NOT NULL default '0',
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`),
  KEY `file_id` (`file_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- 
-- Dumping data for table `downloads`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `features`
-- 

CREATE TABLE `features` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `addon_id` int(11) unsigned NOT NULL default '0',
  `start` datetime NOT NULL default '0000-00-00 00:00:00',
  `end` datetime NOT NULL default '0000-00-00 00:00:00',
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`),
  KEY `addon_id` (`addon_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- 
-- Dumping data for table `features`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `files`
-- 

CREATE TABLE `files` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `version_id` int(11) unsigned NOT NULL default '0',
  `platform_id` int(11) unsigned NOT NULL default '0',
  `filename` varchar(255) NOT NULL,
  `size` int(11) unsigned NOT NULL default '0',
  `hash` varchar(255) default NULL,
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`),
  KEY `version_id` (`version_id`),
  KEY `platform_id` (`platform_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- 
-- Dumping data for table `files`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `langs`
-- 

CREATE TABLE `langs` (
  `id` varchar(255) NOT NULL,
  `name` varchar(255) NOT NULL,
  `meta` varchar(255) NOT NULL,
  `error_text` varchar(255) NOT NULL,
  `encoding` varchar(255) NOT NULL default 'UTF-8',
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 
-- Dumping data for table `langs`
-- 

INSERT INTO `langs` VALUES ('en_US', 'English (US)', 'en_US', 'Error', 'utf-8', '2006-08-28 09:31:03');
INSERT INTO `langs` VALUES ('de_DE', 'German', 'de_DE', 'Störung', 'utf-8', '2006-08-28 09:31:03');

-- --------------------------------------------------------

-- 
-- Table structure for table `platforms`
-- 

CREATE TABLE `platforms` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `name` varchar(255) NOT NULL default '',
  `shortname` varchar(255) NOT NULL default '',
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- 
-- Dumping data for table `platforms`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `previews`
-- 

CREATE TABLE `previews` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `addon_id` int(11) unsigned NOT NULL default '0',
  `filename` varchar(255) NOT NULL default '',
  `caption` varchar(255) NOT NULL default '',
  `highlight` tinyint(1) unsigned NOT NULL default '0',
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`),
  KEY `addon_id` (`addon_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- 
-- Dumping data for table `previews`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `reviewratings`
-- 

CREATE TABLE `reviewratings` (
  `review_id` int(11) unsigned NOT NULL default '0',
  `user_id` int(11) unsigned NOT NULL default '0',
  `helpful` tinyint(1) unsigned NOT NULL default '0',
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`review_id`,`user_id`),
  KEY `user_id` (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- 
-- Dumping data for table `reviewratings`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `reviews`
-- 

CREATE TABLE `reviews` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `version_id` int(11) unsigned NOT NULL default '0',
  `rating` int(11) unsigned NOT NULL default '0',
  `title` varchar(255) NOT NULL default '',
  `body` text NOT NULL,
  `editorreview` tinyint(1) unsigned NOT NULL default '0',
  `flag` tinyint(1) unsigned NOT NULL default '0',
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`),
  KEY `version_id` (`version_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- 
-- Dumping data for table `reviews`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `tags`
-- 

CREATE TABLE `tags` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `name` varchar(255) NOT NULL default '',
  `description` text NOT NULL,
  `addontype_id` int(11) unsigned NOT NULL default '0',
  `application_id` int(11) unsigned NOT NULL default '0',
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`),
  KEY `addontype_id` (`addontype_id`),
  KEY `application_id` (`application_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- 
-- Dumping data for table `tags`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `translations`
-- 

CREATE TABLE `translations` (
  `id` int(11) unsigned NOT NULL,
  `pk_column` varchar(50) NOT NULL,
  `translated_column` varchar(50) NOT NULL,
  `en_US` text,
  `en_GB` text,
  `fr` text,
  `de` text,
  `ja` text,
  `pl` text,
  `es_ES` text,
  `zh_CN` text,
  `zh_TW` text,
  `cs` text,
  `da` text,
  `nl` text,
  `fi` text,
  `hu` text,
  `it` text,
  `ko` text,
  `pt_BR` text,
  `ru` text,
  `es_AR` text,
  `sv_SE` text,
  `tr` text,
  PRIMARY KEY  (`id`,`pk_column`,`translated_column`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 
-- Dumping data for table `translations`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `userevents`
-- 

CREATE TABLE `userevents` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `changed_id` int(11) unsigned NOT NULL,
  `user_id` int(11) unsigned NOT NULL,
  `added` varchar(255) NOT NULL,
  `removed` varchar(255) NOT NULL,
  `notes` text,
  `created` datetime NOT NULL,
  PRIMARY KEY  (`id`),
  KEY `changed_id` (`changed_id`),
  KEY `user_id` (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- 
-- Dumping data for table `userevents`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `users`
-- 

CREATE TABLE `users` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `cake_session_id` varchar(255) default NULL,
  `email` varchar(255) NOT NULL default '',
  `password` varchar(255) NOT NULL,
  `firstname` varchar(255) NOT NULL,
  `lastname` varchar(255) NOT NULL,
  `emailhidden` tinyint(1) unsigned NOT NULL default '0',
  `homepage` varchar(255) default NULL,
  `confirmationcode` varchar(255) NOT NULL default '',
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  `notes` text,
  PRIMARY KEY  (`id`),
  UNIQUE KEY `email` (`email`),
  KEY `cake_session_id` (`cake_session_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- 
-- Dumping data for table `users`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `versions`
-- 

CREATE TABLE `versions` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `addon_id` int(11) unsigned NOT NULL default '0',
  `version` varchar(255) NOT NULL default '',
  `dateadded` datetime NOT NULL default '0000-00-00 00:00:00',
  `dateapproved` datetime NOT NULL default '0000-00-00 00:00:00',
  `dateupdated` datetime NOT NULL default '0000-00-00 00:00:00',
  `approved` tinyint(1) unsigned NOT NULL default '0',
  `releasenotes` text,
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`),
  KEY `addon_id` (`addon_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=1 ;

-- 
-- Dumping data for table `versions`
-- 


-- 
-- Constraints for dumped tables
-- 

-- 
-- Constraints for table `addonevents`
-- 
ALTER TABLE `addonevents`
  ADD CONSTRAINT `addonevents_ibfk_1` FOREIGN KEY (`addon_id`) REFERENCES `addons` (`id`),
  ADD CONSTRAINT `addonevents_ibfk_2` FOREIGN KEY (`user_id`) REFERENCES `users` (`id`);

-- 
-- Constraints for table `addons`
-- 
ALTER TABLE `addons`
  ADD CONSTRAINT `addons_ibfk_1` FOREIGN KEY (`addontype_id`) REFERENCES `addontypes` (`id`);

-- 
-- Constraints for table `addons_tags`
-- 
ALTER TABLE `addons_tags`
  ADD CONSTRAINT `addons_tags_ibfk_3` FOREIGN KEY (`addon_id`) REFERENCES `addons` (`id`),
  ADD CONSTRAINT `addons_tags_ibfk_4` FOREIGN KEY (`tag_id`) REFERENCES `tags` (`id`);

-- 
-- Constraints for table `addons_users`
-- 
ALTER TABLE `addons_users`
  ADD CONSTRAINT `addons_users_ibfk_1` FOREIGN KEY (`addon_id`) REFERENCES `addons` (`id`),
  ADD CONSTRAINT `addons_users_ibfk_2` FOREIGN KEY (`user_id`) REFERENCES `users` (`id`);

-- 
-- Constraints for table `applications_versions`
-- 
ALTER TABLE `applications_versions`
  ADD CONSTRAINT `applications_versions_ibfk_1` FOREIGN KEY (`application_id`) REFERENCES `applications` (`id`),
  ADD CONSTRAINT `applications_versions_ibfk_2` FOREIGN KEY (`version_id`) REFERENCES `versions` (`id`);

-- 
-- Constraints for table `approvals`
-- 
ALTER TABLE `approvals`
  ADD CONSTRAINT `approvals_ibfk_1` FOREIGN KEY (`user_id`) REFERENCES `users` (`id`),
  ADD CONSTRAINT `approvals_ibfk_2` FOREIGN KEY (`file_id`) REFERENCES `files` (`id`);

-- 
-- Constraints for table `appversions`
-- 
ALTER TABLE `appversions`
  ADD CONSTRAINT `appversions_ibfk_1` FOREIGN KEY (`application_id`) REFERENCES `applications` (`id`);

-- 
-- Constraints for table `blapps`
-- 
ALTER TABLE `blapps`
  ADD CONSTRAINT `blapps_ibfk_1` FOREIGN KEY (`blitem_id`) REFERENCES `blitems` (`id`);

-- 
-- Constraints for table `downloads`
-- 
ALTER TABLE `downloads`
  ADD CONSTRAINT `downloads_ibfk_1` FOREIGN KEY (`file_id`) REFERENCES `files` (`id`);

-- 
-- Constraints for table `features`
-- 
ALTER TABLE `features`
  ADD CONSTRAINT `features_ibfk_1` FOREIGN KEY (`addon_id`) REFERENCES `addons` (`id`);

-- 
-- Constraints for table `files`
-- 
ALTER TABLE `files`
  ADD CONSTRAINT `files_ibfk_1` FOREIGN KEY (`version_id`) REFERENCES `versions` (`id`),
  ADD CONSTRAINT `files_ibfk_2` FOREIGN KEY (`platform_id`) REFERENCES `platforms` (`id`);

-- 
-- Constraints for table `previews`
-- 
ALTER TABLE `previews`
  ADD CONSTRAINT `previews_ibfk_1` FOREIGN KEY (`addon_id`) REFERENCES `addons` (`id`);

-- 
-- Constraints for table `reviewratings`
-- 
ALTER TABLE `reviewratings`
  ADD CONSTRAINT `reviewratings_ibfk_1` FOREIGN KEY (`review_id`) REFERENCES `reviews` (`id`),
  ADD CONSTRAINT `reviewratings_ibfk_2` FOREIGN KEY (`user_id`) REFERENCES `users` (`id`);

-- 
-- Constraints for table `reviews`
-- 
ALTER TABLE `reviews`
  ADD CONSTRAINT `reviews_ibfk_1` FOREIGN KEY (`version_id`) REFERENCES `versions` (`id`);

-- 
-- Constraints for table `tags`
-- 
ALTER TABLE `tags`
  ADD CONSTRAINT `tags_ibfk_1` FOREIGN KEY (`addontype_id`) REFERENCES `addontypes` (`id`),
  ADD CONSTRAINT `tags_ibfk_2` FOREIGN KEY (`application_id`) REFERENCES `applications` (`id`);

-- 
-- Constraints for table `userevents`
-- 
ALTER TABLE `userevents`
  ADD CONSTRAINT `userevents_ibfk_1` FOREIGN KEY (`changed_id`) REFERENCES `users` (`id`),
  ADD CONSTRAINT `userevents_ibfk_2` FOREIGN KEY (`user_id`) REFERENCES `users` (`id`);

-- 
-- Constraints for table `users`
-- 
ALTER TABLE `users`
  ADD CONSTRAINT `users_ibfk_1` FOREIGN KEY (`cake_session_id`) REFERENCES `cake_sessions` (`id`);

-- 
-- Constraints for table `versions`
-- 
ALTER TABLE `versions`
  ADD CONSTRAINT `versions_ibfk_1` FOREIGN KEY (`addon_id`) REFERENCES `addons` (`id`);
