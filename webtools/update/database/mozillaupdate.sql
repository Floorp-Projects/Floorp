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
-- version 2.6.0
-- http://www.phpmyadmin.net
-- 
-- Host: localhost
-- Generation Time: Dec 07, 2004 at 02:18 AM
-- Server version: 4.0.21
-- PHP Version: 4.3.8
-- 
-- Database: `mozillaupdate`
-- 

-- --------------------------------------------------------

-- 
-- Table structure for table `applications`
-- 

CREATE TABLE `applications` (
  `AppID` int(11) NOT NULL auto_increment,
  `AppName` varchar(30) NOT NULL default '',
  `Version` varchar(15) NOT NULL default '',
  `major` int(3) NOT NULL default '0',
  `minor` int(3) NOT NULL default '0',
  `release` int(3) NOT NULL default '0',
  `build` int(14) NOT NULL default '0',
  `SubVer` varchar(5) NOT NULL default 'final',
  `GUID` varchar(50) NOT NULL default '',
  `int_version` varchar(5) default NULL,
  `public_ver` enum('YES','NO') NOT NULL default 'YES',
  `shortname` char(2) NOT NULL default '',
  PRIMARY KEY  (`AppID`),
  KEY `AppName` (`AppName`)
) TYPE=InnoDB PACK_KEYS=0;

-- --------------------------------------------------------

-- 
-- Table structure for table `approvallog`
-- 

CREATE TABLE `approvallog` (
  `LogID` int(5) NOT NULL auto_increment,
  `ID` varchar(11) NOT NULL default '',
  `vID` varchar(11) NOT NULL default '',
  `UserID` varchar(11) NOT NULL default '',
  `action` varchar(255) NOT NULL default '',
  `date` datetime NOT NULL default '0000-00-00 00:00:00',
  `Installation` enum('','YES','NO') NOT NULL default '',
  `Uninstallation` enum('','YES','NO') NOT NULL default '',
  `NewChrome` enum('','YES','NO') NOT NULL default '',
  `AppWorks` enum('','YES','NO') NOT NULL default '',
  `VisualErrors` enum('','YES','NO') NOT NULL default '',
  `AllElementsThemed` enum('','YES','NO') NOT NULL default '',
  `CleanProfile` enum('','YES','NO') NOT NULL default '',
  `WorksAsDescribed` enum('','YES','NO') NOT NULL default '',
  `TestBuild` varchar(255) default NULL,
  `TestOS` varchar(255) default NULL,
  `comments` text NOT NULL,
  PRIMARY KEY  (`LogID`),
  KEY `ID` (`ID`),
  KEY `vID` (`vID`),
  KEY `UserID` (`UserID`),
  KEY `UserID_2` (`UserID`)
) TYPE=InnoDB;

-- --------------------------------------------------------

-- 
-- Table structure for table `authorxref`
-- 

CREATE TABLE `authorxref` (
  `ID` int(11) NOT NULL default '0',
  `UserID` int(11) NOT NULL default '0',
  KEY `ID` (`ID`),
  KEY `UserID` (`UserID`)
) TYPE=InnoDB;

-- --------------------------------------------------------

-- 
-- Table structure for table `categories`
-- 

CREATE TABLE `categories` (
  `CategoryID` int(11) NOT NULL auto_increment,
  `CatName` varchar(30) NOT NULL default '',
  `CatDesc` varchar(100) NOT NULL default '',
  `CatType` enum('E','T','P') NOT NULL default 'E',
  `CatApp` varchar(25) NOT NULL default '',
  PRIMARY KEY  (`CategoryID`)
) TYPE=InnoDB;

-- --------------------------------------------------------

-- 
-- Table structure for table `categoryxref`
-- 

CREATE TABLE `categoryxref` (
  `ID` int(11) NOT NULL default '0',
  `CategoryID` int(11) NOT NULL default '0',
  KEY `IDIndex` (`ID`,`CategoryID`),
  KEY `CategoryID` (`CategoryID`)
) TYPE=InnoDB;

-- --------------------------------------------------------

-- 
-- Table structure for table `downloads`
-- 

CREATE TABLE `downloads` (
  `dID` int(11) NOT NULL auto_increment,
  `ID` varchar(5) NOT NULL default '',
  `date` varchar(14) default NULL,
  `downloadcount` int(15) NOT NULL default '0',
  `vID` varchar(5) NOT NULL default '',
  `user_ip` varchar(15) NOT NULL default '',
  `user_agent` text NOT NULL,
  `type` enum('count','download') NOT NULL default 'download',
  PRIMARY KEY  (`dID`),
  KEY `type` (`type`),
  KEY `date` (`date`)
) TYPE=InnoDB PACK_KEYS=0;

-- --------------------------------------------------------

-- 
-- Table structure for table `faq`
-- 

CREATE TABLE `faq` (
  `id` int(3) NOT NULL auto_increment,
  `index` varchar(5) NOT NULL default '1',
  `alias` varchar(20) NOT NULL default '',
  `title` varchar(150) NOT NULL default '',
  `text` text NOT NULL,
  `lastupdated` timestamp(14) NOT NULL,
  `active` enum('YES','NO') NOT NULL default 'YES',
  PRIMARY KEY  (`id`)
) TYPE=InnoDB PACK_KEYS=0;

-- --------------------------------------------------------

-- 
-- Table structure for table `feedback`
-- 

CREATE TABLE `feedback` (
  `CommentID` int(11) NOT NULL auto_increment,
  `ID` int(11) NOT NULL default '0',
  `CommentName` varchar(100) default NULL,
  `CommentVote` enum('0','1','2','3','4','5') default NULL,
  `CommentTitle` varchar(75) NOT NULL default '',
  `CommentNote` text,
  `CommentDate` datetime NOT NULL default '0000-00-00 00:00:00',
  `commentip` varchar(15) NOT NULL default '',
  `email` varchar(128) NOT NULL default '',
  `formkey` varchar(160) NOT NULL default '',
  `helpful-yes` int(6) NOT NULL default '0',
  `helpful-no` int(6) NOT NULL default '0',
  `helpful-rating` varchar(4) NOT NULL default '',
  `VersionTagline` varchar(255) NOT NULL default '',
  `flag` varchar(8) NOT NULL default '',
  PRIMARY KEY  (`CommentID`),
  KEY `ID` (`ID`),
  KEY `CommentDate` (`CommentDate`)
) TYPE=InnoDB PACK_KEYS=0;

-- --------------------------------------------------------

-- 
-- Table structure for table `main`
-- 

CREATE TABLE `main` (
  `ID` int(11) NOT NULL auto_increment,
  `GUID` varchar(50) NOT NULL default '',
  `Name` varchar(100) NOT NULL default '',
  `Type` enum('T','E','P') NOT NULL default 'T',
  `DateAdded` datetime NOT NULL default '0000-00-00 00:00:00',
  `DateUpdated` datetime NOT NULL default '0000-00-00 00:00:00',
  `Homepage` varchar(200) default NULL,
  `Description` text NOT NULL,
  `Rating` varchar(4) NOT NULL default '0',
  `downloadcount` int(15) NOT NULL default '0',
  `TotalDownloads` int(18) NOT NULL default '0',
  `devcomments` text NOT NULL,
  PRIMARY KEY  (`ID`),
  UNIQUE KEY `Name` (`Name`),
  KEY `Type` (`Type`)
) TYPE=InnoDB PACK_KEYS=0;

-- --------------------------------------------------------

-- 
-- Table structure for table `os`
-- 

CREATE TABLE `os` (
  `OSID` int(11) NOT NULL auto_increment,
  `OSName` varchar(20) NOT NULL default '',
  PRIMARY KEY  (`OSID`),
  UNIQUE KEY `OSName` (`OSName`)
) TYPE=InnoDB;

-- 
-- Dumping data for table `os`
-- 

INSERT INTO `os` (`OSID`, `OSName`) VALUES (1, 'ALL');
INSERT INTO `os` (`OSID`, `OSName`) VALUES (4, 'BSD');
INSERT INTO `os` (`OSID`, `OSName`) VALUES (2, 'Linux');
INSERT INTO `os` (`OSID`, `OSName`) VALUES (3, 'MacOSX');
INSERT INTO `os` (`OSID`, `OSName`) VALUES (6, 'Solaris');
INSERT INTO `os` (`OSID`, `OSName`) VALUES (5, 'Windows');

-- --------------------------------------------------------

-- 
-- Table structure for table `previews`
-- 

CREATE TABLE `previews` (
  `PreviewID` int(11) NOT NULL auto_increment,
  `PreviewURI` varchar(200) NOT NULL default '',
  `ID` int(5) NOT NULL default '0',
  `caption` varchar(255) NOT NULL default '',
  `preview` enum('YES','NO') NOT NULL default 'NO',
  PRIMARY KEY  (`PreviewID`),
  KEY `ID` (`ID`)
) TYPE=InnoDB PACK_KEYS=0;

-- --------------------------------------------------------

-- 
-- Table structure for table `reviews`
-- 

CREATE TABLE `reviews` (
  `rID` int(11) NOT NULL auto_increment,
  `ID` int(11) NOT NULL default '0',
  `DateAdded` datetime NOT NULL default '0000-00-00 00:00:00',
  `AuthorID` int(11) NOT NULL default '0',
  `Title` varchar(60) NOT NULL default '',
  `Body` text,
  `ExtendedBody` text NOT NULL,
  `pick` enum('YES','NO') NOT NULL default 'NO',
  `featured` enum('YES','NO') NOT NULL default 'NO',
  `featuredate` varchar(6) NOT NULL default '',
  PRIMARY KEY  (`rID`),
  UNIQUE KEY `ID` (`ID`),
  KEY `AuthorID` (`AuthorID`)
) TYPE=InnoDB PACK_KEYS=0;
        

-- --------------------------------------------------------

-- 
-- Table structure for table `userprofiles`
-- 

CREATE TABLE `userprofiles` (
  `UserID` int(11) NOT NULL auto_increment,
  `UserName` varchar(100) NOT NULL default '',
  `UserEmail` varchar(100) NOT NULL default '',
  `UserWebsite` varchar(100) default NULL,
  `UserPass` varchar(200) NOT NULL default '',
  `UserMode` enum('A','E','U','D') NOT NULL default 'U',
  `UserTrusted` enum('TRUE','FALSE') NOT NULL default 'FALSE',
  `UserEmailHide` tinyint(1) NOT NULL default '1',
  `UserLastLogin` datetime NOT NULL default '0000-00-00 00:00:00',
  `ConfirmationCode` varchar(32) default NULL,
  PRIMARY KEY  (`UserID`),
  UNIQUE KEY `UserEmail` (`UserEmail`)
) TYPE=InnoDB PACK_KEYS=0;

-- --------------------------------------------------------

-- 
-- Table structure for table `version`
-- 

CREATE TABLE `version` (
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
  `approved` enum('YES','NO','?','DISABLED') NOT NULL default '?',
  PRIMARY KEY  (`vID`),
  KEY `ID` (`ID`),
  KEY `AppID` (`AppID`),
  KEY `OSID` (`OSID`),
  KEY `Version` (`Version`)
) TYPE=InnoDB PACK_KEYS=0;

-- 
-- Constraints for dumped tables
-- 

-- 
-- Constraints for table `authorxref`
-- 
ALTER TABLE `authorxref`
  ADD CONSTRAINT `0_125` FOREIGN KEY (`ID`) REFERENCES `main` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `0_126` FOREIGN KEY (`UserID`) REFERENCES `userprofiles` (`UserID`) ON DELETE CASCADE ON UPDATE CASCADE;

-- 
-- Constraints for table `categoryxref`
-- 
ALTER TABLE `categoryxref`
  ADD CONSTRAINT `0_128` FOREIGN KEY (`ID`) REFERENCES `main` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `0_129` FOREIGN KEY (`CategoryID`) REFERENCES `categories` (`CategoryID`) ON DELETE CASCADE ON UPDATE CASCADE;

-- 
-- Constraints for table `feedback`
-- 
ALTER TABLE `feedback`
  ADD CONSTRAINT `0_131` FOREIGN KEY (`ID`) REFERENCES `main` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

-- 
-- Constraints for table `previews`
-- 
ALTER TABLE `previews`
  ADD CONSTRAINT `previews_ibfk_1` FOREIGN KEY (`ID`) REFERENCES `main` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

-- 
-- Constraints for table `reviews`
-- 
ALTER TABLE `reviews`
  ADD CONSTRAINT `0_135` FOREIGN KEY (`ID`) REFERENCES `main` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `0_136` FOREIGN KEY (`AppID`) REFERENCES `applications` (`AppID`) ON DELETE CASCADE ON UPDATE CASCADE;

-- 
-- Constraints for table `version`
-- 
ALTER TABLE `version`
  ADD CONSTRAINT `0_139` FOREIGN KEY (`ID`) REFERENCES `main` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `version_ibfk_1` FOREIGN KEY (`AppID`) REFERENCES `applications` (`AppID`) ON UPDATE CASCADE;
