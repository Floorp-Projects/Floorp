#!/usr/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is Litmus.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Zach Lipton <zach@zachlipton.com>

use strict;

BEGIN {
	unless (-e "data/") {
    	system("mkdir data/");
    }
    unless (-e "localconfig") {
         open(OUT, ">localconfig");
         print OUT <<EOT;
our \$db_host = "";
our \$db_name = "";
our \$db_user = "";
our \$db_pass = "";

our \$user_cookiename = "litmus_login";
our \$sysconfig_cookiename = "litmustestingconfiguration";
EOT
        close(OUT);
        exit;
    }
}

use Litmus::DB::Product; 
use Litmus::DB::Testgroup; 
use Litmus::DB::Subgroup; 
use Litmus::DB::Status; 
use Litmus::DB::Platform; 
use Litmus::DB::Opsys; 
use Litmus::DB::Branch;
use Litmus::DB::Format; 
use Litmus::DB::User;
use Litmus::Cache;

# formats
Litmus::DB::Format->find_or_create({name => "manual-steps"}); 

# users
Litmus::DB::User->find_or_create({email => 'web-tester@mozilla.org', istrusted => 0});

# products
my %products;
$products{"firefox"} = Litmus::DB::Product->find_or_create({name => "Firefox", iconpath => "ff.gif"});
$products{"seamonkey"} = Litmus::DB::Product->find_or_create({name => "Seamonkey", iconpath => "moz.png"});
$products{"thunderbird"} = Litmus::DB::Product->find_or_create({name => "Thunderbird", iconpath => "tb.gif"});

# branches
Litmus::DB::Branch->find_or_create({product => $products{"firefox"}, 
                            name => "Trunk", 
                            detect_regexp => "Firefox/1\\.6a1"});
Litmus::DB::Branch->find_or_create({product => $products{"firefox"}, 
                            name => "1.5 Branch",
                            detect_regexp => "Firefox/1\.(0\\+|4)"});
Litmus::DB::Branch->find_or_create({product => $products{"firefox"}, 
                            name => "1.0.x Branch",
                            detect_regexp => "Firefox/1\\.0\\."});
                            
Litmus::DB::Branch->find_or_create({product => $products{"seamonkey"}, 
                            name => "Trunk", 
                            detect_regexp => "."});

Litmus::DB::Branch->find_or_create({product => $products{"thunderbird"}, 
                            name => "Trunk",
                            detect_regexp => "."});
Litmus::DB::Branch->find_or_create({product => $products{"thunderbird"}, 
                            name => "1.0.x Branch"},
                            detect_regexp => "");


# testgroups
my %testgroups;
$testgroups{"ff_st"} = Litmus::DB::Testgroup->find_or_create({product => $products{"firefox"}, 
                               name => "Smoketests", 
                               expirationdays => 5});
$testgroups{"ff_bft"} = Litmus::DB::Testgroup->find_or_create({product => $products{"firefox"}, 
                               name => "Basic Functional Tests", 
                               expirationdays => 10});
                               
$testgroups{"sm_ias"} = Litmus::DB::Testgroup->find_or_create({product => $products{"seamonkey"}, 
                               name => "Installation and Startup",
                               expirationdays => 5});
$testgroups{"sm_mb"} = Litmus::DB::Testgroup->find_or_create({product => $products{"seamonkey"}, 
                               name => "Menu bar",
                               expirationdays => 5});

$testgroups{"tb_st"} = Litmus::DB::Testgroup->find_or_create({product => $products{"thunderbird"}, 
                               name => "Smoketests",
                               expirationdays => 5});
$testgroups{"tb_bft"} = Litmus::DB::Testgroup->find_or_create({product => $products{"thunderbird"}, 
                               name => "Basic Functional Tests",
                               expirationdays => 5});

                               
# subgroups
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_st"}, 
                              name => "Install and Startup"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_st"}, 
                              name => "Functionality"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_st"}, 
                              name => "Uninstall"});
                              
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Installation"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Migration"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Menu Bar"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Help"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Page Controls"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Toolbar Customization"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Location Bar and Autocomplete"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Search"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Printing"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Tabbed Browsing"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Options (Preferences)"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Popup and annoyance blocking"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Find in Page"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Bookmarks"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "History"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Downloading"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Extensions"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Theme Manager"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "JavaScript Console"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "DOM Inspector"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Page Info"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Password Manager"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Form Manager"});    
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Cookies"});    
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Top Sites"});    
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "View Source"});    
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Software Update"});    
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Security and Certs"});    
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Import"});    
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Plug Ins"});    
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Security"});    
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"ff_bft"}, 
                              name => "Uninstall"});    
                                                            
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"sm_ias"}, 
                              name => "Launch"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"sm_mb"}, 
                              name => "IE Migration"});
                              
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_st"}, 
                              name => "Install and Startup"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_st"}, 
                              name => "Account Creation"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_st"}, 
                              name => "Message Viewing"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_st"}, 
                              name => "Message Composition"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_st"}, 
                              name => "Address Book"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_st"}, 
                              name => "General"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_st"}, 
                              name => "Shutdown"});
                              
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Install, shutdown, uninstall"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Migration"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Updating Thunderbird"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Import"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Window configuration"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Toolbars and menus "});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Account settings & Preferences (Options)"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "IMAP accounts"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "POP accounts (exclude Global Inbox)"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Global inbox"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Mail composition"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Spell checker"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "RSS account & subscriptions"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Newsgroups"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Navigating and displaying messages"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Downloading and saving"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Image blocking"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Return receipts"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Proxies"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Offline, disk space"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Moving, copying, deleting messages"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Views and labeling messages"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Message filters"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Message search"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Address search"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Virtual folders"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Message Grouping"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Quicksearch"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Address books"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Junk mail"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Extensions"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Theme management"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Help"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Printing"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Master Passwords & password management"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Phishing, spoof detection"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Secure connections"});
Litmus::DB::Subgroup->find_or_create({testgroup => $testgroups{"tb_bft"},
                name => "Digital signing, encrypting message"});


# statuses
Litmus::DB::Status->find_or_create({name => "Enabled"});
Litmus::DB::Status->find_or_create({name => "Disabled"});
Litmus::DB::Status->find_or_create({name => "Unconfirmed"});

#platforms
my %platforms;
$platforms{"ff_Windows"} = Litmus::DB::Platform->find_or_create({name    => "Windows", 
                                                                 product => $products{"firefox"}, 
                                                                 detect_regexp  => "Windows",
                                                                 iconpath => "win.png"});
$platforms{"ff_Linux"} = Litmus::DB::Platform->find_or_create({name => "Linux",
                                                               product => $products{"firefox"},
                                                               detect_regexp  => "Linux",
                                                               iconpath => "lin.png"});
$platforms{"ff_Mac"} = Litmus::DB::Platform->find_or_create({name    => "Mac",
                                                             product => $products{"firefox"},
                                                             detect_regexp  => "Macintosh",
                                                            iconpath => "mac.png"});

$platforms{"sm_Windows"} = Litmus::DB::Platform->find_or_create({name    => "Windows",
                                                                 product => $products{"seamonkey"},
                                                                 detect_regexp  => "Windows",
                                                                iconpath => "win.png"});
$platforms{"sm_Linux"} = Litmus::DB::Platform->find_or_create({name    => "Linux",
                                                               product => $products{"seamonkey"},
                                                               detect_regexp  => "Linux",
                                                            iconpath => "lin.png"});
$platforms{"sm_Mac"} = Litmus::DB::Platform->find_or_create({name    => "Mac", 
                                                             product => $products{"seamonkey"},
                                                             detect_regexp  => "Macintosh",
                                                            iconpath => "mac.png"});

$platforms{"tb_Windows"} = Litmus::DB::Platform->find_or_create({name    => "Windows",
                                                                 product => $products{"thunderbird"},
                                                                 detect_regexp  => "Windows",
                                                            iconpath => "win.png"});
$platforms{"tb_Linux"} = Litmus::DB::Platform->find_or_create({name    => "Linux",
                                                               product => $products{"thunderbird"},
                                                               detect_regexp  => "Linux",
                                                            iconpath => "lin.png"});
$platforms{"tb_Mac"} = Litmus::DB::Platform->find_or_create({name    => "Mac", 
                                                             product => $products{"thunderbird"},
                                                             detect_regexp  => "Macintosh",
                                                            iconpath => "mac.png"});

# opsyses
Litmus::DB::Opsys->find_or_create({platform => $platforms{"ff_Windows"}, 
                           name => "Windows 98"});
Litmus::DB::Opsys->find_or_create({platform => $platforms{"ff_Windows"}, 
                           name => "Windows ME"});
Litmus::DB::Opsys->find_or_create({platform => $platforms{"ff_Windows"}, 
                           name => "Windows 2000"});
Litmus::DB::Opsys->find_or_create({platform => $platforms{"ff_Windows"}, 
                           name => "Windows XP"});
                           
Litmus::DB::Opsys->find_or_create({platform => $platforms{"ff_Linux"}, 
                           name => "Linux"});
                           
Litmus::DB::Opsys->find_or_create({platform => $platforms{"ff_Mac"}, 
                           name => "Mac OS 10.2"});
Litmus::DB::Opsys->find_or_create({platform => $platforms{"ff_Mac"}, 
                           name => "Mac OS 10.3"});
Litmus::DB::Opsys->find_or_create({platform => $platforms{"ff_Mac"}, 
                           name => "Mac OS 10.4"});

Litmus::DB::Opsys->find_or_create({platform => $platforms{"sm_Windows"}, 
                           name => "Windows 98"});
Litmus::DB::Opsys->find_or_create({platform => $platforms{"sm_Windows"}, 
                           name => "Windows ME"});
Litmus::DB::Opsys->find_or_create({platform => $platforms{"sm_Windows"}, 
                           name => "Windows 2000"});
Litmus::DB::Opsys->find_or_create({platform => $platforms{"sm_Windows"}, 
                           name => "Windows XP"});
                           
Litmus::DB::Opsys->find_or_create({platform => $platforms{"sm_Linux"}, 
                           name => "Linux"});
                           
Litmus::DB::Opsys->find_or_create({platform => $platforms{"sm_Mac"}, 
                           name => "Mac OS 10.2"});
Litmus::DB::Opsys->find_or_create({platform => $platforms{"sm_Mac"}, 
                           name => "Mac OS 10.3"});
Litmus::DB::Opsys->find_or_create({platform => $platforms{"sm_Mac"}, 
                           name => "Mac OS 10.4"});

Litmus::DB::Opsys->find_or_create({platform => $platforms{"tb_Windows"}, 
                           name => "Windows 98"});
Litmus::DB::Opsys->find_or_create({platform => $platforms{"tb_Windows"}, 
                           name => "Windows ME"});
Litmus::DB::Opsys->find_or_create({platform => $platforms{"tb_Windows"}, 
                           name => "Windows 2000"});
Litmus::DB::Opsys->find_or_create({platform => $platforms{"tb_Windows"}, 
                           name => "Windows XP"});
                           
Litmus::DB::Opsys->find_or_create({platform => $platforms{"tb_Linux"}, 
                           name => "Linux"});
                           
Litmus::DB::Opsys->find_or_create({platform => $platforms{"tb_Mac"}, 
                           name => "Mac OS 10.2"});
Litmus::DB::Opsys->find_or_create({platform => $platforms{"tb_Mac"}, 
                           name => "Mac OS 10.3"});
Litmus::DB::Opsys->find_or_create({platform => $platforms{"tb_Mac"}, 
                           name => "Mac OS 10.4"});
                           
# results:
Litmus::DB::Result->find_or_create(name => "Pass", style => "background-color: #00FF00;");
Litmus::DB::Result->find_or_create(name => "Fail", style => "background-color: #FF0000;");
Litmus::DB::Result->find_or_create(name => "Test unclear/broken", style => "background-color: #FFFF66;");

# javascript cache                  
rebuildCache(); 

