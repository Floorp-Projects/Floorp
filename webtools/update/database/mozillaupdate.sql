# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http:#www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Update.
#
# The Initial Developer of the Original Code is
# Chris "Wolf" Crews.
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chris "Wolf" Crews <psychoticwolf@carolina.rr.com>
#   Alan Starr <alanjstarr@yahoo.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****


-- phpMyAdmin SQL Dump
-- version 2.6.0-rc1
-- http://www.phpmyadmin.net
-- 
-- Host: localhost
-- Generation Time: Sep 12, 2004 at 05:07 AM
-- Server version: 4.0.18
-- PHP Version: 4.3.8
-- 
-- Database: `mozillaupdate`
-- 

-- --------------------------------------------------------

-- 
-- Table structure for table `t_applications`
-- 

CREATE TABLE `t_applications` (
  `AppID` int(11) NOT NULL auto_increment,
  `AppName` varchar(30) NOT NULL default '',
  `Version` varchar(10) NOT NULL default '',
  `major` int(3) NOT NULL default '0',
  `minor` int(3) NOT NULL default '0',
  `release` int(3) NOT NULL default '0',
  `build` int(14) NOT NULL default '0',
  `SubVer` enum('a','b','final','','+') NOT NULL default 'final',
  `GUID` varchar(50) NOT NULL default '',
  PRIMARY KEY  (`AppID`),
  KEY `AppName` (`AppName`)
) TYPE=InnoDB PACK_KEYS=0 AUTO_INCREMENT=25 ;

-- --------------------------------------------------------

-- 
-- Table structure for table `t_approvallog`
-- 

CREATE TABLE `t_approvallog` (
  `LogID` int(5) NOT NULL auto_increment,
  `ID` varchar(11) NOT NULL default '',
  `vID` varchar(11) NOT NULL default '',
  `UserID` varchar(11) NOT NULL default '',
  `action` varchar(255) NOT NULL default '',
  `date` datetime NOT NULL default '0000-00-00 00:00:00',
  `comments` text NOT NULL,
  PRIMARY KEY  (`LogID`),
  KEY `ID` (`ID`),
  KEY `vID` (`vID`),
  KEY `UserID` (`UserID`),
  KEY `UserID_2` (`UserID`)
) TYPE=InnoDB AUTO_INCREMENT=430 ;

-- --------------------------------------------------------

-- 
-- Table structure for table `t_authorxref`
-- 

CREATE TABLE `t_authorxref` (
  `ID` int(11) NOT NULL default '0',
  `UserID` int(11) NOT NULL default '0',
  KEY `ID` (`ID`),
  KEY `UserID` (`UserID`)
) TYPE=InnoDB;

-- --------------------------------------------------------

-- 
-- Table structure for table `t_categories`
-- 

CREATE TABLE `t_categories` (
  `CategoryID` int(11) NOT NULL auto_increment,
  `CatName` varchar(30) NOT NULL default '',
  `CatDesc` varchar(100) NOT NULL default '',
  `CatType` enum('E','T','P') NOT NULL default 'E',
  PRIMARY KEY  (`CategoryID`)
) TYPE=InnoDB AUTO_INCREMENT=28 ;

-- --------------------------------------------------------

-- 
-- Table structure for table `t_categoryxref`
-- 

CREATE TABLE `t_categoryxref` (
  `ID` int(11) NOT NULL default '0',
  `CategoryID` int(11) NOT NULL default '0',
  KEY `IDIndex` (`ID`,`CategoryID`),
  KEY `CategoryID` (`CategoryID`)
) TYPE=InnoDB;

-- --------------------------------------------------------

-- 
-- Table structure for table `t_downloads`
-- 

CREATE TABLE `t_downloads` (
  `dID` int(11) NOT NULL auto_increment,
  `ID` varchar(5) NOT NULL default '',
  `date` varchar(14) default NULL,
  `downloadcount` int(15) NOT NULL default '0',
  `vID` varchar(5) NOT NULL default '',
  `user_ip` varchar(15) NOT NULL default '',
  `user_agent` text NOT NULL,
  `type` enum('count','download') NOT NULL default 'download',
  PRIMARY KEY  (`dID`)
) TYPE=InnoDB PACK_KEYS=0 AUTO_INCREMENT=3 ;

-- --------------------------------------------------------

-- 
-- Table structure for table `t_faq`
-- 

CREATE TABLE `t_faq` (
  `id` int(3) NOT NULL auto_increment,
  `index` varchar(5) NOT NULL default '1',
  `alias` varchar(12) NOT NULL default '',
  `title` varchar(150) NOT NULL default '',
  `text` text NOT NULL,
  `lastupdated` timestamp(14) NOT NULL,
  `active` enum('YES','NO') NOT NULL default 'YES',
  PRIMARY KEY  (`id`)
) TYPE=InnoDB PACK_KEYS=0 AUTO_INCREMENT=7 ;

-- --------------------------------------------------------

-- 
-- Table structure for table `t_feedback`
-- 

CREATE TABLE `t_feedback` (
  `CommentID` int(11) NOT NULL auto_increment,
  `ID` int(11) NOT NULL default '0',
  `CommentName` varchar(100) default NULL,
  `CommentVote` enum('0','1','2','3','4','5') default NULL,
  `CommentTitle` varchar(75) NOT NULL default '',
  `CommentNote` text,
  `CommentDate` datetime NOT NULL default '0000-00-00 00:00:00',
  `commentip` varchar(15) NOT NULL default '',
  PRIMARY KEY  (`CommentID`),
  KEY `ID` (`ID`)
) TYPE=InnoDB PACK_KEYS=0 AUTO_INCREMENT=15487 ;

-- --------------------------------------------------------

-- 
-- Table structure for table `t_main`
-- 

CREATE TABLE `t_main` (
  `ID` int(11) NOT NULL auto_increment,
  `GUID` varchar(50) NOT NULL default '',
  `Name` varchar(100) NOT NULL default '',
  `Type` enum('T','E','P') NOT NULL default 'T',
  `DateAdded` datetime NOT NULL default '0000-00-00 00:00:00',
  `DateUpdated` datetime NOT NULL default '0000-00-00 00:00:00',
  `Homepage` varchar(200) default NULL,
  `Description` varchar(255) NOT NULL default '',
  `Rating` varchar(4) NOT NULL default '0',
  `downloadcount` int(15) NOT NULL default '0',
  `TotalDownloads` int(18) NOT NULL default '0',
  PRIMARY KEY  (`ID`),
  KEY `Type` (`Type`)
) TYPE=InnoDB PACK_KEYS=0 AUTO_INCREMENT=218 ;

-- --------------------------------------------------------

-- 
-- Table structure for table `t_os`
-- 

CREATE TABLE `t_os` (
  `OSID` int(11) NOT NULL auto_increment,
  `OSName` varchar(20) NOT NULL default '',
  PRIMARY KEY  (`OSID`),
  UNIQUE KEY `OSName` (`OSName`)
) TYPE=InnoDB AUTO_INCREMENT=7 ;

-- --------------------------------------------------------

-- 
-- Table structure for table `t_previews`
-- 

CREATE TABLE `t_previews` (
  `PreviewID` int(11) NOT NULL auto_increment,
  `PreviewURI` varchar(200) NOT NULL default '',
  `vID` int(11) NOT NULL default '0',
  PRIMARY KEY  (`PreviewID`),
  KEY `vID` (`vID`)
) TYPE=InnoDB PACK_KEYS=0 AUTO_INCREMENT=24 ;

-- --------------------------------------------------------

-- 
-- Table structure for table `t_reviews`
-- 

CREATE TABLE `t_reviews` (
  `rID` int(11) NOT NULL auto_increment,
  `ID` int(11) NOT NULL default '0',
  `AppID` int(11) NOT NULL default '0',
  `DateAdded` datetime NOT NULL default '0000-00-00 00:00:00',
  `AuthorID` int(11) NOT NULL default '0',
  `Title` varchar(60) NOT NULL default '',
  `Body` text,
  `pick` enum('YES','NO') NOT NULL default 'NO',
  `featured` enum('YES','NO') NOT NULL default 'NO',
  PRIMARY KEY  (`rID`),
  KEY `ID` (`ID`),
  KEY `AppID` (`AppID`),
  KEY `AuthorID` (`AuthorID`)
) TYPE=InnoDB PACK_KEYS=0 AUTO_INCREMENT=3 ;

-- --------------------------------------------------------

-- 
-- Table structure for table `t_userprofiles`
-- 

CREATE TABLE `t_userprofiles` (
  `UserID` int(11) NOT NULL auto_increment,
  `UserName` varchar(100) NOT NULL default '',
  `UserEmail` varchar(100) NOT NULL default '',
  `UserWebsite` varchar(100) default NULL,
  `UserPass` varchar(200) NOT NULL default '',
  `UserMode` enum('A','E','U','D') NOT NULL default 'U',
  `UserTrusted` enum('TRUE','FALSE') NOT NULL default 'FALSE',
  `UserEmailHide` tinyint(1) NOT NULL default '1',
  PRIMARY KEY  (`UserID`),
  UNIQUE KEY `UserEmail` (`UserEmail`)
) TYPE=InnoDB PACK_KEYS=0 AUTO_INCREMENT=142 ;

-- --------------------------------------------------------

-- 
-- Table structure for table `t_version`
-- 

CREATE TABLE `t_version` (
  `vID` int(11) NOT NULL auto_increment,
  `ID` int(11) NOT NULL default '0',
  `Version` varchar(30) NOT NULL default '0',
  `OSID` int(11) NOT NULL default '0',
  `AppID` int(11) NOT NULL default '0',
  `MinAppVer` varchar(10) NOT NULL default '',
  `MinAppVer_int` varchar(10) NOT NULL default '',
  `MaxAppVer` varchar(10) NOT NULL default '',
  `MaxAppVer_int` varchar(10) NOT NULL default '',
  `Size` int(11) NOT NULL default '0',
  `DateAdded` datetime NOT NULL default '0000-00-00 00:00:00',
  `DateUpdated` datetime NOT NULL default '0000-00-00 00:00:00',
  `URI` varchar(255) NOT NULL default '',
  `Notes` text,
  `approved` enum('YES','NO','?') NOT NULL default '?',
  PRIMARY KEY  (`vID`),
  KEY `ID` (`ID`),
  KEY `AppID` (`AppID`),
  KEY `OSID` (`OSID`),
  KEY `Version` (`Version`)
) TYPE=InnoDB PACK_KEYS=0 AUTO_INCREMENT=558 ;

-- 
-- Constraints for dumped tables
-- 

-- 
-- Constraints for table `t_authorxref`
-- 
ALTER TABLE `t_authorxref`
  ADD CONSTRAINT `0_125` FOREIGN KEY (`ID`) REFERENCES `t_main` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `0_126` FOREIGN KEY (`UserID`) REFERENCES `t_userprofiles` (`UserID`) ON DELETE CASCADE ON UPDATE CASCADE;

-- 
-- Constraints for table `t_categoryxref`
-- 
ALTER TABLE `t_categoryxref`
  ADD CONSTRAINT `0_128` FOREIGN KEY (`ID`) REFERENCES `t_main` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `0_129` FOREIGN KEY (`CategoryID`) REFERENCES `t_categories` (`CategoryID`) ON DELETE CASCADE ON UPDATE CASCADE;

-- 
-- Constraints for table `t_feedback`
-- 
ALTER TABLE `t_feedback`
  ADD CONSTRAINT `0_131` FOREIGN KEY (`ID`) REFERENCES `t_main` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

-- 
-- Constraints for table `t_previews`
-- 
ALTER TABLE `t_previews`
  ADD CONSTRAINT `0_133` FOREIGN KEY (`vID`) REFERENCES `t_version` (`vID`) ON DELETE CASCADE ON UPDATE CASCADE;

-- 
-- Constraints for table `t_reviews`
-- 
ALTER TABLE `t_reviews`
  ADD CONSTRAINT `0_135` FOREIGN KEY (`ID`) REFERENCES `t_main` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `0_136` FOREIGN KEY (`AppID`) REFERENCES `t_applications` (`AppID`) ON DELETE CASCADE ON UPDATE CASCADE;

-- 
-- Constraints for table `t_version`
-- 
ALTER TABLE `t_version`
  ADD CONSTRAINT `0_139` FOREIGN KEY (`ID`) REFERENCES `t_main` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `0_140` FOREIGN KEY (`AppID`) REFERENCES `t_applications` (`AppID`) ON DELETE CASCADE ON UPDATE CASCADE;
