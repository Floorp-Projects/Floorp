-- --------------------------------------------------------

-- 
-- Table structure for table `applications`
-- 

DROP TABLE IF EXISTS `applications`;
CREATE TABLE `applications` (
  `AppID` int(11) NOT NULL auto_increment,
  `AppName` varchar(30) NOT NULL default '',
  `Version` varchar(10) NOT NULL default '',
  `major` int(3) NOT NULL default '0',
  `minor` int(3) NOT NULL default '0',
  `release` int(3) NOT NULL default '0',
  `build` int(14) NOT NULL default '0',
  `SubVer` varchar(15) NOT NULL default 'final',
  `GUID` varchar(50) NOT NULL default '',
  `int_version` varchar(5) default NULL,
  `public_ver` enum('YES','NO') NOT NULL default 'YES',
  `shortname` char(2) NOT NULL default '',
  PRIMARY KEY  (`AppID`),
  KEY `AppName` (`AppName`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 PACK_KEYS=0;

-- --------------------------------------------------------

-- 
-- Table structure for table `approvallog`
-- 

DROP TABLE IF EXISTS `approvallog`;
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
  `TestBuild` varchar(255) NOT NULL default '',
  `TestOS` varchar(255) NOT NULL default '',
  `comments` text NOT NULL,
  PRIMARY KEY  (`LogID`),
  KEY `ID` (`ID`),
  KEY `vID` (`vID`),
  KEY `UserID` (`UserID`),
  KEY `UserID_2` (`UserID`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

-- 
-- Table structure for table `authorxref`
-- 

DROP TABLE IF EXISTS `authorxref`;
CREATE TABLE `authorxref` (
  `ID` int(11) NOT NULL default '0',
  `UserID` int(11) NOT NULL default '0',
  KEY `ID` (`ID`),
  KEY `UserID` (`UserID`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

-- 
-- Table structure for table `categories`
-- 

DROP TABLE IF EXISTS `categories`;
CREATE TABLE `categories` (
  `CategoryID` int(11) NOT NULL auto_increment,
  `CatName` varchar(30) NOT NULL default '',
  `CatDesc` varchar(100) NOT NULL default '',
  `CatType` enum('E','T','P') NOT NULL default 'E',
  `CatApp` varchar(25) NOT NULL default '',
  PRIMARY KEY  (`CategoryID`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

-- 
-- Table structure for table `categoryxref`
-- 

DROP TABLE IF EXISTS `categoryxref`;
CREATE TABLE `categoryxref` (
  `ID` int(11) NOT NULL default '0',
  `CategoryID` int(11) NOT NULL default '0',
  KEY `IDIndex` (`ID`,`CategoryID`),
  KEY `CategoryID` (`CategoryID`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

-- 
-- Table structure for table `downloads`
-- 

DROP TABLE IF EXISTS `downloads`;
CREATE TABLE `downloads` (
  `dID` int(11) NOT NULL auto_increment,
  `ID` int(11) unsigned default NULL,
  `date` datetime default NULL,
  `downloadcount` int(15) NOT NULL default '0',
  `vID` int(11) unsigned default NULL,
  `user_ip` varchar(15) NOT NULL default '',
  `user_agent` varchar(255) default NULL,
  `type` enum('count','download') NOT NULL default 'download',
  `counted` int(1) unsigned NOT NULL default '0',
  PRIMARY KEY  (`dID`),
  KEY `date` (`date`),
  KEY `dup_download_check_idx` (`ID`,`date`,`vID`,`user_ip`,`user_agent`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 PACK_KEYS=0;

-- --------------------------------------------------------

-- 
-- Table structure for table `faq`
-- 

DROP TABLE IF EXISTS `faq`;
CREATE TABLE `faq` (
  `id` int(3) NOT NULL auto_increment,
  `index` varchar(5) NOT NULL default '1',
  `alias` varchar(12) NOT NULL default '',
  `title` varchar(150) NOT NULL default '',
  `text` text NOT NULL,
  `lastupdated` timestamp NOT NULL default CURRENT_TIMESTAMP on update CURRENT_TIMESTAMP,
  `active` enum('YES','NO') NOT NULL default 'YES',
  PRIMARY KEY  (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 PACK_KEYS=0;

-- --------------------------------------------------------

-- 
-- Table structure for table `feedback`
-- 

DROP TABLE IF EXISTS `feedback`;
CREATE TABLE `feedback` (
  `CommentID` int(11) NOT NULL auto_increment,
  `ID` int(11) NOT NULL default '0',
  `CommentName` varchar(100) default NULL,
  `CommentVote` tinyint(3) unsigned default NULL,
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
  KEY `ID` (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 PACK_KEYS=0;

-- --------------------------------------------------------

-- 
-- Table structure for table `feedback_ipbans`
-- 

DROP TABLE IF EXISTS `feedback_ipbans`;
CREATE TABLE `feedback_ipbans` (
  `bID` int(11) NOT NULL auto_increment,
  `beginip` varchar(15) NOT NULL default '',
  `endip` varchar(15) NOT NULL default '',
  `DateAdded` datetime default '0000-00-00 00:00:00',
  `comments` text NOT NULL,
  PRIMARY KEY  (`bID`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

-- 
-- Table structure for table `main`
-- 

DROP TABLE IF EXISTS `main`;
CREATE TABLE `main` (
  `ID` int(11) NOT NULL auto_increment,
  `GUID` varchar(50) NOT NULL default '',
  `Name` varchar(100) NOT NULL default '',
  `Type` enum('T','E','P') NOT NULL default 'T',
  `DateAdded` datetime NOT NULL default '0000-00-00 00:00:00',
  `DateUpdated` datetime NOT NULL default '0000-00-00 00:00:00',
  `Homepage` varchar(200) default NULL,
  `Description` text NOT NULL,
  `Rating` varchar(4) default NULL,
  `downloadcount` int(15) NOT NULL default '0',
  `TotalDownloads` int(18) NOT NULL default '0',
  `devcomments` text NOT NULL,
  PRIMARY KEY  (`ID`),
  KEY `Type` (`Type`),
  KEY `GUID` (`GUID`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 PACK_KEYS=0;

-- --------------------------------------------------------

-- 
-- Table structure for table `os`
-- 

DROP TABLE IF EXISTS `os`;
CREATE TABLE `os` (
  `OSID` int(11) NOT NULL auto_increment,
  `OSName` varchar(20) NOT NULL default '',
  PRIMARY KEY  (`OSID`),
  UNIQUE KEY `OSName` (`OSName`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

-- 
-- Table structure for table `previews`
-- 

DROP TABLE IF EXISTS `previews`;
CREATE TABLE `previews` (
  `PreviewID` int(11) NOT NULL auto_increment,
  `PreviewURI` varchar(200) NOT NULL default '',
  `ID` int(5) NOT NULL default '0',
  `caption` varchar(255) NOT NULL default '',
  `preview` enum('YES','NO') NOT NULL default 'NO',
  PRIMARY KEY  (`PreviewID`),
  KEY `ID` (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 PACK_KEYS=0;

-- --------------------------------------------------------

-- 
-- Table structure for table `reviews`
-- 

DROP TABLE IF EXISTS `reviews`;
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
  UNIQUE KEY `ID_2` (`ID`),
  KEY `ID` (`ID`),
  KEY `AuthorID` (`AuthorID`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 PACK_KEYS=0;

-- --------------------------------------------------------

-- 
-- Table structure for table `userprofiles`
-- 

DROP TABLE IF EXISTS `userprofiles`;
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
  `ConfirmationCode` varchar(32) NOT NULL default '',
  PRIMARY KEY  (`UserID`),
  UNIQUE KEY `UserEmail` (`UserEmail`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1 PACK_KEYS=0;

-- --------------------------------------------------------

-- 
-- Table structure for table `version`
-- 

DROP TABLE IF EXISTS `version`;
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
) ENGINE=InnoDB DEFAULT CHARSET=latin1 PACK_KEYS=0;

-- 
-- Constraints for dumped tables
-- 

-- 
-- Constraints for table `authorxref`
-- 
ALTER TABLE `authorxref`
  ADD CONSTRAINT `0_21` FOREIGN KEY (`ID`) REFERENCES `main` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `0_22` FOREIGN KEY (`UserID`) REFERENCES `userprofiles` (`UserID`) ON DELETE CASCADE ON UPDATE CASCADE;

-- 
-- Constraints for table `categoryxref`
-- 
ALTER TABLE `categoryxref`
  ADD CONSTRAINT `0_25` FOREIGN KEY (`ID`) REFERENCES `main` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `0_26` FOREIGN KEY (`CategoryID`) REFERENCES `categories` (`CategoryID`) ON DELETE CASCADE ON UPDATE CASCADE;

-- 
-- Constraints for table `feedback`
-- 
ALTER TABLE `feedback`
  ADD CONSTRAINT `feedback_ibfk_1` FOREIGN KEY (`ID`) REFERENCES `main` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

-- 
-- Constraints for table `previews`
-- 
ALTER TABLE `previews`
  ADD CONSTRAINT `previews_ibfk_1` FOREIGN KEY (`ID`) REFERENCES `main` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE;

-- 
-- Constraints for table `reviews`
-- 
ALTER TABLE `reviews`
  ADD CONSTRAINT `0_40` FOREIGN KEY (`ID`) REFERENCES `main` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `0_42` FOREIGN KEY (`AuthorID`) REFERENCES `userprofiles` (`UserID`) ON DELETE CASCADE ON UPDATE CASCADE;

-- 
-- Constraints for table `version`
-- 
ALTER TABLE `version`
  ADD CONSTRAINT `0_34` FOREIGN KEY (`ID`) REFERENCES `main` (`ID`) ON DELETE CASCADE ON UPDATE CASCADE,
  ADD CONSTRAINT `version_ibfk_1` FOREIGN KEY (`OSID`) REFERENCES `os` (`OSID`) ON UPDATE CASCADE,
  ADD CONSTRAINT `version_ibfk_2` FOREIGN KEY (`AppID`) REFERENCES `applications` (`AppID`) ON UPDATE CASCADE;
        
