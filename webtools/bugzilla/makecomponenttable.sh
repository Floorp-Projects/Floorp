#!/bin/sh
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
# 
# The Original Code is the Bugzilla Bug Tracking System.
# 
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.
# 
# Contributor(s): Terry Weissman <terry@mozilla.org>

mysql > /dev/null 2>/dev/null << OK_ALL_DONE

use bugs;

drop table components
OK_ALL_DONE

mysql << OK_ALL_DONE
use bugs;
create table components (
value tinytext,
program tinytext,
initialowner tinytext not null,	# Should arguably be a mediumint!
initialqacontact tinytext not null # Should arguably be a mediumint!
);



insert into components (value, program, initialowner, initialqacontact) values ("XPFC", "Calendar", "spider@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("config", "Calendar", "spider@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Core", "Calendar", "sman@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("NLS", "Calendar", "jusn@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("UI", "Calendar", "eyork@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Test", "Calendar", "sman@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Install", "Calendar", "sman@netscape.com", "");

insert into components (value, program, initialowner, initialqacontact) values ("CCK-Wizard", "CCK", "selmer@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("CCK-Installation", "CCK", "selmer@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("CCK-Shell", "CCK", "selmer@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("CCK-Whitebox", "CCK", "selmer@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Dialup-Install", "CCK", "selmer@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Dialup-Account Setup", "CCK", "selmer@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Dialup-Mup/Muc", "CCK", "selmer@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Dialup-Upgrade", "CCK", "selmer@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("AS-Whitebox", "CCK", "selmer@netscape.com", "");

insert into components (value, program, initialowner, initialqacontact) values ("LDAP C SDK", "Directory", "chuckb@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("LDAP Java SDK", "Directory", "chuckb@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("PerLDAP", "Directory", "leif@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("LDAP Tools", "Directory", "chuckb@netscape.com", "");


insert into components (value, program, initialowner, initialqacontact) values ("Networking", "MailNews", "mscott@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Database", "MailNews", "davidmc@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("MIME", "MailNews", "rhp@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Security", "MailNews", "jefft@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Composition", "MailNews", "ducarroz@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Address Book", "MailNews", "putterman@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Front End", "MailNews", "warren@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Back End", "MailNews", "phil@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Internationalization", "MailNews", "nhotta@netscape.com", "momoi@netscape.com");
insert into components (value, program, initialowner, initialqacontact) values ("Localization", "MailNews", "rchen@netscape.com", "momoi@netscape.com");



insert into components (value, program, initialowner, initialqacontact) values ("Macintosh FE", "Mozilla", "sdagley@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Windows FE", "Mozilla", "blythe@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("XFE", "Mozilla", "ramiro@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("StubFE", "Mozilla", "rickg@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Aurora/RDF FE", "Mozilla", "don@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Aurora/RDF BE", "Mozilla", "guha@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Berkeley DB", "Mozilla", "montulli@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Browser Hooks", "Mozilla", "ebina@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Build Config", "Mozilla", "briano@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Composer", "Mozilla", "akkana@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Compositor Library", "Mozilla", "vidur@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Dialup", "Mozilla", "selmer@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("FontLib", "Mozilla", "dp@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("HTML Dialogs", "Mozilla", "nisheeth@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("HTML to Text/PostScript Translation", "Mozilla", "brendan@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("ImageLib", "Mozilla", "pnunn@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("JPEG Image Handling", "Mozilla", "tgl@sss.pgh.pa.us", "");
insert into components (value, program, initialowner, initialqacontact) values ("PNG Image Handling", "Mozilla", "png@wco.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Image Conversion Library", "Mozilla", "mjudge@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("I18N Library", "Mozilla", "bobj@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Java Stubs", "Mozilla", "warren@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("JavaScript", "Mozilla", "mccabe@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("JavaScript/Java Reflection", "Mozilla", "fur@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Layout", "Mozilla", "rickg@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("LibMocha", "Mozilla", "mlm@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("MIMELib", "Mozilla", "terry@mozilla.org", "");
insert into components (value, program, initialowner, initialqacontact) values ("NetLib", "Mozilla", "gagan@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("NSPR", "Mozilla", "srinivas@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Password Cache", "Mozilla", "montulli@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("PICS", "Mozilla", "montulli@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Plugins", "Mozilla", "amusil@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Preferences", "Mozilla", "aoki@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Progress Window", "Mozilla", "atotic@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Registry", "Mozilla", "dveditz@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Scheduler", "Mozilla", "aoki@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Security Stubs", "Mozilla", "jsw@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("SmartUpdate", "Mozilla", "dveditz@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("XML", "Mozilla", "guha@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("XP-COM", "Mozilla", "scullin@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("XP File Handling", "Mozilla", "atotic@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("XP Miscellany", "Mozilla", "brendan@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("XP Utilities", "Mozilla", "rickg@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Zlib", "Mozilla", "pnunn@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Internationalization", "Mozilla", "ftang@netscape.com", "teruko@netscape.com");
insert into components (value, program, initialowner, initialqacontact) values ("Localization", "Mozilla", "rchen@netscape.com", "teruko@netscape.com");



insert into components (value, program, initialowner, initialqacontact) values ("Platform: Lesstif on Linux", "Mozilla", "ramiro@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Platform: OS/2", "Mozilla", "law@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Platform: MacOS/PPC", "Mozilla", "sdagley@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Platform: Rhapsody", "Mozilla", "mcafee@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Platform: MFC/Win32 on Windows", "Mozilla", "blythe@netscape.com", "");


insert into components (value, program, initialowner, initialqacontact) values ("ActiveX Wrapper", "NGLayout", "locka@iol.ie", "");
insert into components (value, program, initialowner, initialqacontact) values ("Content Model", "NGLayout", "kipp@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Cookies", "NGLayout", "scullin@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("DOM", "NGLayout", "vidur@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("DOM", "NGLayout", "vidur@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Editor", "NGLayout", "kostello@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Embedding APIs", "NGLayout", "jevering@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Event Handling", "NGLayout", "joki@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Event Handling", "NGLayout", "joki@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Form Submission", "NGLayout", "pollmann@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("HTMLFrames", "NGLayout", "karnaze@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("HTMLTables", "NGLayout", "karnaze@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Layout", "NGLayout", "troy@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Networking Library", "NGLayout", "gagan@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Parser", "NGLayout", "rickg@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Plug-ins", "NGLayout", "amusil@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Rendering", "NGLayout", "michaelp@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Selection and Search", "NGLayout", "kostello@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Style System", "NGLayout", "peterl@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Threading", "NGLayout", "rpotts@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Viewer App", "NGLayout", "rickg@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Widget Set", "NGLayout", "kmcclusk@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("XPCOM", "NGLayout", "scullin@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("xpidl", "NGLayout", "shaver@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Internationalization", "NGLayout", "ftang@netscape.com", "teruko@netscape.com");
insert into components (value, program, initialowner, initialqacontact) values ("Localization", "NGLayout", "rchen@netscape.com", "teruko@netscape.com");


insert into components (value, program, initialowner, initialqacontact) values ("Bonsai", "Webtools", "terry@mozilla.org", "");
insert into components (value, program, initialowner, initialqacontact) values ("Bugzilla", "Webtools", "terry@mozilla.org", "");
insert into components (value, program, initialowner, initialqacontact) values ("Despot", "Webtools", "terry@mozilla.org", "");
insert into components (value, program, initialowner, initialqacontact) values ("LXR", "Webtools", "endico@mozilla.org", "");
insert into components (value, program, initialowner, initialqacontact) values ("Mozbot", "Webtools", "terry@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Tinderbox", "Webtools", "terry@mozilla.org", "");


select * from components;

show columns from components;
show index from components;

OK_ALL_DONE
