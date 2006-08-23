-- phpMyAdmin SQL Dump
-- version 2.8.2.1
-- http://www.phpmyadmin.net
-- 
-- Host: localhost
-- Generation Time: Aug 23, 2006 at 02:57 PM
-- Server version: 4.1.20
-- PHP Version: 4.3.9
-- 
-- Database: `fligtar`
-- 

-- --------------------------------------------------------

-- 
-- Table structure for table `addons`
-- 

DROP TABLE IF EXISTS `addons`;
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
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=7 ;

-- 
-- Dumping data for table `addons`
-- 

INSERT INTO `addons` (`id`, `guid`, `name`, `addontype_id`, `icon`, `homepage`, `description`, `summary`, `averagerating`, `weeklydownloads`, `totaldownloads`, `developercomments`, `inactive`, `prerelease`, `adminreview`, `sitespecific`, `externalsoftware`, `approvalnotes`, `eula`, `privacypolicy`, `created`, `modified`) VALUES (1, 'en-AU@dictionaries.addons.mozilla.org', 'English (Australian)', 3, '', 'http://justcameron.com/incoming/en-au-dictionary/', 'Are you sick of all your favo-U-rite colo-U-rful language being marked incorrect? Me too. Live no more in the world of crazy American spelling - get the Australian English Dictionary :)', 'Australian English Dictionary', '5', 123, 1234, 'Please be aware that this dictionary is designed for Firefox and Thunderbird versions that are not yet released. It WILL NOT WORK with Fx or Tb 1.5.0.* (if you don''t know what version you''re using, then it won''t work.) This dictionary is targeted at developers and testers of Firefox and Thunderbird 2 Alphas and Betas - if you are not one, please do not bother downloading it.\r\n\r\nCurrently free support is not available - paid support is available at AUS$100 per second.\r\n\r\nDistributed under the GNU General Public License - see license.txt for more details.', 0, 0, 0, 0, 0, 'Please approve ASAP. I almost thought I misspelled Oodnadatta.', '		    GNU GENERAL PUBLIC LICENSE\r\n		       Version 2, June 1991\r\n\r\n Copyright (C) 1989, 1991 Free Software Foundation, Inc.,\r\n 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA\r\n Everyone is permitted to copy and distribute verbatim copies\r\n of this license document, but changing it is not allowed.\r\n\r\n			    Preamble\r\n\r\n  The licenses for most software are designed to take away your\r\nfreedom to share and change it.  By contrast, the GNU General Public\r\nLicense is intended to guarantee your freedom to share and change free\r\nsoftware--to make sure the software is free for all its users.  This\r\nGeneral Public License applies to most of the Free Software\r\nFoundation''s software and to any other program whose authors commit to\r\nusing it.  (Some other Free Software Foundation software is covered by\r\nthe GNU Lesser General Public License instead.)  You can apply it to\r\nyour programs, too.\r\n\r\n  When we speak of free software, we are referring to freedom, not\r\nprice.  Our General Public Licenses are designed to make sure that you\r\nhave the freedom to distribute copies of free software (and charge for\r\nthis service if you wish), that you receive source code or can get it\r\nif you want it, that you can change the software or use pieces of it\r\nin new free programs; and that you know you can do these things.\r\n\r\n  To protect your rights, we need to make restrictions that forbid\r\nanyone to deny you these rights or to ask you to surrender the rights.\r\nThese restrictions translate to certain responsibilities for you if you\r\ndistribute copies of the software, or if you modify it.\r\n\r\n  For example, if you distribute copies of such a program, whether\r\ngratis or for a fee, you must give the recipients all the rights that\r\nyou have.  You must make sure that they, too, receive or can get the\r\nsource code.  And you must show them these terms so they know their\r\nrights.\r\n\r\n  We protect your rights with two steps: (1) copyright the software, and\r\n(2) offer you this license which gives you legal permission to copy,\r\ndistribute and/or modify the software.\r\n\r\n  Also, for each author''s protection and ours, we want to make certain\r\nthat everyone understands that there is no warranty for this free\r\nsoftware.  If the software is modified by someone else and passed on, we\r\nwant its recipients to know that what they have is not the original, so\r\nthat any problems introduced by others will not reflect on the original\r\nauthors'' reputations.\r\n\r\n  Finally, any free program is threatened constantly by software\r\npatents.  We wish to avoid the danger that redistributors of a free\r\nprogram will individually obtain patent licenses, in effect making the\r\nprogram proprietary.  To prevent this, we have made it clear that any\r\npatent must be licensed for everyone''s free use or not licensed at all.\r\n\r\n  The precise terms and conditions for copying, distribution and\r\nmodification follow.\r\n\r\n		    GNU GENERAL PUBLIC LICENSE\r\n   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION\r\n\r\n  0. This License applies to any program or other work which contains\r\na notice placed by the copyright holder saying it may be distributed\r\nunder the terms of this General Public License.  The "Program", below,\r\nrefers to any such program or work, and a "work based on the Program"\r\nmeans either the Program or any derivative work under copyright law:\r\nthat is to say, a work containing the Program or a portion of it,\r\neither verbatim or with modifications and/or translated into another\r\nlanguage.  (Hereinafter, translation is included without limitation in\r\nthe term "modification".)  Each licensee is addressed as "you".\r\n\r\nActivities other than copying, distribution and modification are not\r\ncovered by this License; they are outside its scope.  The act of\r\nrunning the Program is not restricted, and the output from the Program\r\nis covered only if its contents constitute a work based on the\r\nProgram (independent of having been made by running the Program).\r\nWhether that is true depends on what the Program does.\r\n\r\n  1. You may copy and distribute verbatim copies of the Program''s\r\nsource code as you receive it, in any medium, provided that you\r\nconspicuously and appropriately publish on each copy an appropriate\r\ncopyright notice and disclaimer of warranty; keep intact all the\r\nnotices that refer to this License and to the absence of any warranty;\r\nand give any other recipients of the Program a copy of this License\r\nalong with the Program.\r\n\r\nYou may charge a fee for the physical act of transferring a copy, and\r\nyou may at your option offer warranty protection in exchange for a fee.\r\n\r\n  2. You may modify your copy or copies of the Program or any portion\r\nof it, thus forming a work based on the Program, and copy and\r\ndistribute such modifications or work under the terms of Section 1\r\nabove, provided that you also meet all of these conditions:\r\n\r\n    a) You must cause the modified files to carry prominent notices\r\n    stating that you changed the files and the date of any change.\r\n\r\n    b) You must cause any work that you distribute or publish, that in\r\n    whole or in part contains or is derived from the Program or any\r\n    part thereof, to be licensed as a whole at no charge to all third\r\n    parties under the terms of this License.\r\n\r\n    c) If the modified program normally reads commands interactively\r\n    when run, you must cause it, when started running for such\r\n    interactive use in the most ordinary way, to print or display an\r\n    announcement including an appropriate copyright notice and a\r\n    notice that there is no warranty (or else, saying that you provide\r\n    a warranty) and that users may redistribute the program under\r\n    these conditions, and telling the user how to view a copy of this\r\n    License.  (Exception: if the Program itself is interactive but\r\n    does not normally print such an announcement, your work based on\r\n    the Program is not required to print an announcement.)\r\n\r\nThese requirements apply to the modified work as a whole.  If\r\nidentifiable sections of that work are not derived from the Program,\r\nand can be reasonably considered independent and separate works in\r\nthemselves, then this License, and its terms, do not apply to those\r\nsections when you distribute them as separate works.  But when you\r\ndistribute the same sections as part of a whole which is a work based\r\non the Program, the distribution of the whole must be on the terms of\r\nthis License, whose permissions for other licensees extend to the\r\nentire whole, and thus to each and every part regardless of who wrote it.\r\n\r\nThus, it is not the intent of this section to claim rights or contest\r\nyour rights to work written entirely by you; rather, the intent is to\r\nexercise the right to control the distribution of derivative or\r\ncollective works based on the Program.\r\n\r\nIn addition, mere aggregation of another work not based on the Program\r\nwith the Program (or with a work based on the Program) on a volume of\r\na storage or distribution medium does not bring the other work under\r\nthe scope of this License.\r\n\r\n  3. You may copy and distribute the Program (or a work based on it,\r\nunder Section 2) in object code or executable form under the terms of\r\nSections 1 and 2 above provided that you also do one of the following:\r\n\r\n    a) Accompany it with the complete corresponding machine-readable\r\n    source code, which must be distributed under the terms of Sections\r\n    1 and 2 above on a medium customarily used for software interchange; or,\r\n\r\n    b) Accompany it with a written offer, valid for at least three\r\n    years, to give any third party, for a charge no more than your\r\n    cost of physically performing source distribution, a complete\r\n    machine-readable copy of the corresponding source code, to be\r\n    distributed under the terms of Sections 1 and 2 above on a medium\r\n    customarily used for software interchange; or,\r\n\r\n    c) Accompany it with the information you received as to the offer\r\n    to distribute corresponding source code.  (This alternative is\r\n    allowed only for noncommercial distribution and only if you\r\n    received the program in object code or executable form with such\r\n    an offer, in accord with Subsection b above.)\r\n\r\nThe source code for a work means the preferred form of the work for\r\nmaking modifications to it.  For an executable work, complete source\r\ncode means all the source code for all modules it contains, plus any\r\nassociated interface definition files, plus the scripts used to\r\ncontrol compilation and installation of the executable.  However, as a\r\nspecial exception, the source code distributed need not include\r\nanything that is normally distributed (in either source or binary\r\nform) with the major components (compiler, kernel, and so on) of the\r\noperating system on which the executable runs, unless that component\r\nitself accompanies the executable.\r\n\r\nIf distribution of executable or object code is made by offering\r\naccess to copy from a designated place, then offering equivalent\r\naccess to copy the source code from the same place counts as\r\ndistribution of the source code, even though third parties are not\r\ncompelled to copy the source along with the object code.\r\n\r\n  4. You may not copy, modify, sublicense, or distribute the Program\r\nexcept as expressly provided under this License.  Any attempt\r\notherwise to copy, modify, sublicense or distribute the Program is\r\nvoid, and will automatically terminate your rights under this License.\r\nHowever, parties who have received copies, or rights, from you under\r\nthis License will not have their licenses terminated so long as such\r\nparties remain in full compliance.\r\n\r\n  5. You are not required to accept this License, since you have not\r\nsigned it.  However, nothing else grants you permission to modify or\r\ndistribute the Program or its derivative works.  These actions are\r\nprohibited by law if you do not accept this License.  Therefore, by\r\nmodifying or distributing the Program (or any work based on the\r\nProgram), you indicate your acceptance of this License to do so, and\r\nall its terms and conditions for copying, distributing or modifying\r\nthe Program or works based on it.\r\n\r\n  6. Each time you redistribute the Program (or any work based on the\r\nProgram), the recipient automatically receives a license from the\r\noriginal licensor to copy, distribute or modify the Program subject to\r\nthese terms and conditions.  You may not impose any further\r\nrestrictions on the recipients'' exercise of the rights granted herein.\r\nYou are not responsible for enforcing compliance by third parties to\r\nthis License.\r\n\r\n  7. If, as a consequence of a court judgment or allegation of patent\r\ninfringement or for any other reason (not limited to patent issues),\r\nconditions are imposed on you (whether by court order, agreement or\r\notherwise) that contradict the conditions of this License, they do not\r\nexcuse you from the conditions of this License.  If you cannot\r\ndistribute so as to satisfy simultaneously your obligations under this\r\nLicense and any other pertinent obligations, then as a consequence you\r\nmay not distribute the Program at all.  For example, if a patent\r\nlicense would not permit royalty-free redistribution of the Program by\r\nall those who receive copies directly or indirectly through you, then\r\nthe only way you could satisfy both it and this License would be to\r\nrefrain entirely from distribution of the Program.\r\n\r\nIf any portion of this section is held invalid or unenforceable under\r\nany particular circumstance, the balance of the section is intended to\r\napply and the section as a whole is intended to apply in other\r\ncircumstances.\r\n\r\nIt is not the purpose of this section to induce you to infringe any\r\npatents or other property right claims or to contest validity of any\r\nsuch claims; this section has the sole purpose of protecting the\r\nintegrity of the free software distribution system, which is\r\nimplemented by public license practices.  Many people have made\r\ngenerous contributions to the wide range of software distributed\r\nthrough that system in reliance on consistent application of that\r\nsystem; it is up to the author/donor to decide if he or she is willing\r\nto distribute software through any other system and a licensee cannot\r\nimpose that choice.\r\n\r\nThis section is intended to make thoroughly clear what is believed to\r\nbe a consequence of the rest of this License.\r\n\r\n  8. If the distribution and/or use of the Program is restricted in\r\ncertain countries either by patents or by copyrighted interfaces, the\r\noriginal copyright holder who places the Program under this License\r\nmay add an explicit geographical distribution limitation excluding\r\nthose countries, so that distribution is permitted only in or among\r\ncountries not thus excluded.  In such case, this License incorporates\r\nthe limitation as if written in the body of this License.\r\n\r\n  9. The Free Software Foundation may publish revised and/or new versions\r\nof the General Public License from time to time.  Such new versions will\r\nbe similar in spirit to the present version, but may differ in detail to\r\naddress new problems or concerns.\r\n\r\nEach version is given a distinguishing version number.  If the Program\r\nspecifies a version number of this License which applies to it and "any\r\nlater version", you have the option of following the terms and conditions\r\neither of that version or of any later version published by the Free\r\nSoftware Foundation.  If the Program does not specify a version number of\r\nthis License, you may choose any version ever published by the Free Software\r\nFoundation.\r\n\r\n  10. If you wish to incorporate parts of the Program into other free\r\nprograms whose distribution conditions are different, write to the author\r\nto ask for permission.  For software which is copyrighted by the Free\r\nSoftware Foundation, write to the Free Software Foundation; we sometimes\r\nmake exceptions for this.  Our decision will be guided by the two goals\r\nof preserving the free status of all derivatives of our free software and\r\nof promoting the sharing and reuse of software generally.\r\n\r\n			    NO WARRANTY\r\n\r\n  11. BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY\r\nFOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW.  EXCEPT WHEN\r\nOTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES\r\nPROVIDE THE PROGRAM "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED\r\nOR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF\r\nMERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS\r\nTO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE\r\nPROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING,\r\nREPAIR OR CORRECTION.\r\n\r\n  12. IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING\r\nWILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR\r\nREDISTRIBUTE THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES,\r\nINCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING\r\nOUT OF THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED\r\nTO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY\r\nYOU OR THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER\r\nPROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE\r\nPOSSIBILITY OF SUCH DAMAGES.\r\n\r\n		     END OF TERMS AND CONDITIONS\r\n', NULL, '2006-08-22 13:18:44', '2006-08-22 13:18:44'),
(2, 'winfox@ms.com', 'Winfox', 1, '', 'ms.com', 'To fully integrate Firefox with windows, please install this extension. You''ll see a noticeable slowdown of Firefox and and increased crash rate. Enjoy the talking paperclip too :)', 'Make Firefox more like Windows.', '0', 3, 3, 'Please be aware that after a trial period of 30 minutes, you will have to pay to use this extension.', 0, 0, 1, 0, 0, NULL, 'In exchange for the use of this extension, your soul is forfeit.', 'This extension steals all your passwords and then deletes your harddrive, don''t complain about it.', '2006-08-22 13:24:42', '2006-08-22 13:24:42'),
(3, 'winfox@ms.com', 'Winfox', 1, '', 'ms.com', 'To fully integrate Firefox with windows, please install this extension. You''ll see a noticeable slowdown of Firefox and and increased crash rate. Enjoy the talking paperclip too :)', 'Make Firefox more like Windows.', '0', 3, 3, 'Please be aware that after a trial period of 30 minutes, you will have to pay to use this extension.', 0, 0, 0, 0, 0, NULL, 'In exchange for the use of this extension, your soul is forfeit.', 'This extension steals all your passwords and then deletes your harddrive, don''t complain about it.', '2006-08-22 13:25:53', '2006-08-22 13:25:53'),
(4, 'testheme@amo.org', 'Invisible', 2, '', 'invisible.com', 'Hey, there''s an idea.. lets make every themed element white and see what users think of it. Good luck trying to change your theme back to the default!', 'A white theme.', '3', 561370, 5712902, 'No support is available.', 0, 0, 0, 0, 0, 'See the funny side? Don''t deny this just on principle.', '', '', '2006-08-22 22:21:56', '2006-08-22 22:22:30'),
(5, 'testgooglesearchplugin@amo.org', 'Google search', 4, '', '', 'So what if Google is the default search engine? Lets put another one in.', 'Google search, mark two.', '5', 4294967295, 4294967295, NULL, 0, 1, 0, 1, 0, NULL, NULL, NULL, '2006-08-22 22:35:59', '2006-08-22 22:35:59'),
(6, 'iapple@apple.com', 'iTranslated', 5, '', 'apple.com', 'Get hip! This add-on will translate Firefox into apple''s new iLanguage. It doesn''t actually do that yet, but apple users all over the world are getting hyped about it. Synch it with your ipod, ichicken and stay icool.', '', NULL, 0, 0, NULL, 0, 1, 0, 0, 0, NULL, NULL, NULL, '2006-08-23 01:24:30', '2006-08-23 01:24:30');

-- --------------------------------------------------------

-- 
-- Table structure for table `addons_tags`
-- 

DROP TABLE IF EXISTS `addons_tags`;
CREATE TABLE `addons_tags` (
  `addon_id` int(11) unsigned NOT NULL default '0',
  `tag_id` int(11) unsigned NOT NULL default '0',
  PRIMARY KEY  (`addon_id`,`tag_id`),
  KEY `tag_id` (`tag_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- 
-- Dumping data for table `addons_tags`
-- 

INSERT INTO `addons_tags` (`addon_id`, `tag_id`) VALUES (2, 2),
(3, 2),
(5, 4),
(4, 7);

-- --------------------------------------------------------

-- 
-- Table structure for table `addons_users`
-- 

DROP TABLE IF EXISTS `addons_users`;
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

INSERT INTO `addons_users` (`addon_id`, `user_id`, `created`, `modified`) VALUES (1, 1, '0000-00-00 00:00:00', '0000-00-00 00:00:00'),
(2, 3, '0000-00-00 00:00:00', '0000-00-00 00:00:00'),
(3, 3, '0000-00-00 00:00:00', '0000-00-00 00:00:00'),
(4, 1, '0000-00-00 00:00:00', '0000-00-00 00:00:00'),
(5, 1, '0000-00-00 00:00:00', '0000-00-00 00:00:00'),
(6, 4, '0000-00-00 00:00:00', '0000-00-00 00:00:00');

-- --------------------------------------------------------

-- 
-- Table structure for table `addontypes`
-- 

DROP TABLE IF EXISTS `addontypes`;
CREATE TABLE `addontypes` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `name` varchar(255) NOT NULL default '',
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=7 ;

-- 
-- Dumping data for table `addontypes`
-- 

INSERT INTO `addontypes` (`id`, `name`, `created`, `modified`) VALUES (1, 'Extension', '2006-08-21 23:53:19', '2006-08-21 23:53:19'),
(2, 'Theme', '2006-08-21 23:53:24', '2006-08-21 23:53:24'),
(3, 'Dictionary', '2006-08-21 23:53:30', '2006-08-21 23:53:30'),
(4, 'Search Plugin', '2006-08-21 23:53:36', '2006-08-21 23:53:36'),
(5, 'Language Pack (Application)', '2006-08-21 23:53:58', '2006-08-21 23:53:58'),
(6, 'Language Pack (Add-on)', '2006-08-21 23:54:09', '2006-08-21 23:54:09');

-- --------------------------------------------------------

-- 
-- Table structure for table `applications`
-- 

DROP TABLE IF EXISTS `applications`;
CREATE TABLE `applications` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `guid` varchar(255) NOT NULL default '',
  `name` varchar(255) NOT NULL default '',
  `shortname` varchar(255) NOT NULL default '',
  `supported` tinyint(1) NOT NULL default '0',
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  PRIMARY KEY  (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=8 ;

-- 
-- Dumping data for table `applications`
-- 

INSERT INTO `applications` (`id`, `guid`, `name`, `shortname`, `supported`, `created`, `modified`) VALUES (1, '{ec8030f7-c20a-464f-9b0e-13a3a9e97384}', 'Mozilla Firefox', 'Firefox', 1, '2006-08-22 11:18:35', '2006-08-22 11:18:35'),
(2, '{3550f703-e582-4d05-9a08-453d09bdfdc6}', 'Mozilla Thunderbird', 'Thunderbird', 1, '0000-00-00 00:00:00', '2006-08-22 11:19:48'),
(3, '{86c18b42-e466-45a9-ae7a-9b95ba6f5640}', 'Mozilla Suite', 'Mozilla', 1, '2006-08-22 11:20:48', '2006-08-22 11:20:48'),
(4, '{92650c4d-4b8e-4d2a-b7eb-24ecf4f6b63a}', 'SeaMonkey', 'SeaMonkey', 0, '2006-08-22 11:23:10', '2006-08-22 11:23:10'),
(5, '{136c295a-4a5a-41cf-bf24-5cee526720d5}', 'NVU', 'NVU', 0, '2006-08-22 12:18:14', '2006-08-22 12:18:14'),
(6, '{718e30fb-e89b-41dd-9da7-e25a45638b28}', 'Sunbird', 'Sunbird', 0, '2006-08-22 12:19:45', '2006-08-22 12:19:45'),
(7, '{a463f10c-3994-11da-9945-000d60ca027b}', 'Flock', 'Flock', 0, '2006-08-22 12:29:15', '2006-08-22 12:29:15');

-- --------------------------------------------------------

-- 
-- Table structure for table `applications_versions`
-- 

DROP TABLE IF EXISTS `applications_versions`;
CREATE TABLE `applications_versions` (
  `application_id` int(11) unsigned NOT NULL default '0',
  `version_id` int(11) unsigned NOT NULL default '0',
  `min` varchar(255) NOT NULL default '',
  `max` varchar(255) NOT NULL default '',
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

DROP TABLE IF EXISTS `approvals`;
CREATE TABLE `approvals` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `user_id` int(11) unsigned NOT NULL default '0',
  `action` varchar(255) NOT NULL default '',
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

DROP TABLE IF EXISTS `appversions`;
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

DROP TABLE IF EXISTS `blapps`;
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

DROP TABLE IF EXISTS `blitems`;
CREATE TABLE `blitems` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `guid` varchar(255) NOT NULL default '',
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

DROP TABLE IF EXISTS `cake_sessions`;
CREATE TABLE `cake_sessions` (
  `id` varchar(255) NOT NULL default '',
  `data` text,
  `expires` int(11) default NULL,
  PRIMARY KEY  (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- 
-- Dumping data for table `cake_sessions`
-- 

INSERT INTO `cake_sessions` (`id`, `data`, `expires`) VALUES ('1', 'fasfsdfsd', 13123);

-- --------------------------------------------------------

-- 
-- Table structure for table `downloads`
-- 

DROP TABLE IF EXISTS `downloads`;
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

DROP TABLE IF EXISTS `features`;
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

DROP TABLE IF EXISTS `files`;
CREATE TABLE `files` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `version_id` int(11) unsigned NOT NULL default '0',
  `platform_id` int(11) unsigned NOT NULL default '0',
  `filename` varchar(255) NOT NULL default '',
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

DROP TABLE IF EXISTS `langs`;
CREATE TABLE `langs` (
  `id` varchar(255) NOT NULL default '',
  `name` varchar(255) NOT NULL default '',
  `meta` varchar(255) NOT NULL default '',
  `error_text` varchar(255) NOT NULL default '',
  PRIMARY KEY  (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 
-- Dumping data for table `langs`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `platforms`
-- 

DROP TABLE IF EXISTS `platforms`;
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

DROP TABLE IF EXISTS `previews`;
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

DROP TABLE IF EXISTS `reviewratings`;
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

DROP TABLE IF EXISTS `reviews`;
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

DROP TABLE IF EXISTS `tags`;
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
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=10 ;

-- 
-- Dumping data for table `tags`
-- 

INSERT INTO `tags` (`id`, `name`, `description`, `addontype_id`, `application_id`, `created`, `modified`) VALUES (1, 'Developer tools', 'Tools for developers.', 1, 1, '2006-08-22 12:27:42', '2006-08-22 12:51:46'),
(2, 'Viruses', 'Declare your extension as a virus by listing it under this category', 1, 1, '2006-08-22 12:35:06', '2006-08-22 12:35:06'),
(3, 'News reading', 'For reading the news in Thunderbird. ', 1, 2, '2006-08-22 12:47:47', '2006-08-22 12:47:47'),
(4, 'Google', 'For all plugins relating to Google.', 4, 1, '2006-08-22 12:48:20', '2006-08-22 12:48:20'),
(5, 'Brushed metal', 'Everyone''s copying Safari, so lets lump em all into one category.', 2, 1, '2006-08-22 12:51:07', '2006-08-22 12:51:07'),
(6, 'Brushed metal', 'Everyone''s copying Safari, so lets lump em all into one category.', 2, 2, '2006-08-22 12:51:33', '2006-08-22 12:51:33'),
(7, 'Humour', 'For funny themes', 2, 1, '2006-08-22 22:19:58', '2006-08-22 22:19:58'),
(8, 'Light on dark', '', 2, 2, '2006-08-22 22:20:20', '2006-08-22 22:20:20'),
(9, 'Dark on Light', '', 2, 2, '2006-08-22 22:22:56', '2006-08-22 22:22:56');

-- --------------------------------------------------------

-- 
-- Table structure for table `translations`
-- 

DROP TABLE IF EXISTS `translations`;
CREATE TABLE `translations` (
  `id` int(11) unsigned NOT NULL default '0',
  `pk_column` varchar(50) NOT NULL default '',
  `translated_column` varchar(50) NOT NULL default '',
  `en-US` text,
  `en-GB` text,
  `fr` text,
  `de` text,
  `ja` text,
  `pl` text,
  `es-ES` text,
  `zh-CN` text,
  `zh-TW` text,
  `cs` text,
  `da` text,
  `nl` text,
  `fi` text,
  `hu` text,
  `it` text,
  `ko` text,
  `pt-BR` text,
  `ru` text,
  `es-AR` text,
  `sv-SE` text,
  `tr` text,
  PRIMARY KEY  (`id`,`pk_column`,`translated_column`)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- 
-- Dumping data for table `translations`
-- 


-- --------------------------------------------------------

-- 
-- Table structure for table `users`
-- 

DROP TABLE IF EXISTS `users`;
CREATE TABLE `users` (
  `id` int(11) unsigned NOT NULL auto_increment,
  `cake_session_id` varchar(255) default NULL,
  `email` varchar(255) NOT NULL default '',
  `password` varchar(255) NOT NULL default '',
  `firstname` varchar(255) NOT NULL default '',
  `lastname` varchar(255) NOT NULL default '',
  `emailhidden` tinyint(1) unsigned NOT NULL default '0',
  `homepage` varchar(255) default NULL,
  `confirmationcode` varchar(255) NOT NULL default '',
  `created` datetime NOT NULL default '0000-00-00 00:00:00',
  `modified` datetime NOT NULL default '0000-00-00 00:00:00',
  `notes` text,
  PRIMARY KEY  (`id`),
  UNIQUE KEY `email` (`email`),
  KEY `cake_session_id` (`cake_session_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 AUTO_INCREMENT=5 ;

-- 
-- Dumping data for table `users`
-- 

INSERT INTO `users` (`id`, `cake_session_id`, `email`, `password`, `firstname`, `lastname`, `emailhidden`, `homepage`, `confirmationcode`, `created`, `modified`, `notes`) VALUES (1, '1', 'testing@justcameron.com', 'testing', 'Cameron', 'R', 0, '', 'logmein', '2006-08-22 00:07:32', '2006-08-22 11:07:20', 'First user! woot!'),
(2, '1', 'fligtar@gmail.com', 'poo', 'justin', 'scott', 1, 'sfj', '2312', '2006-08-22 00:13:05', '2006-08-22 00:13:05', '123'),
(3, '1', 'bill@ms.com', 'logmein', 'Bill', 'Gates', 1, 'microsoft.com', 'confirm', '2006-08-22 11:08:45', '2006-08-22 11:08:45', NULL),
(4, '1', 'steve@apple.com', 'ipod', 'Steve', 'Jobs', 0, 'apple.com', 'confirmed', '2006-08-23 01:21:07', '2006-08-23 01:21:07', NULL);

-- --------------------------------------------------------

-- 
-- Table structure for table `versions`
-- 

DROP TABLE IF EXISTS `versions`;
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
  `modifid` datetime NOT NULL default '0000-00-00 00:00:00',
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
-- Constraints for table `users`
-- 
ALTER TABLE `users`
  ADD CONSTRAINT `users_ibfk_1` FOREIGN KEY (`cake_session_id`) REFERENCES `cake_sessions` (`id`);

-- 
-- Constraints for table `versions`
-- 
ALTER TABLE `versions`
  ADD CONSTRAINT `versions_ibfk_1` FOREIGN KEY (`addon_id`) REFERENCES `addons` (`id`);
