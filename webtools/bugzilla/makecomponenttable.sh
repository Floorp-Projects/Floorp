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
initialowner tinytext           # Should arguably be a mediumint!
);




insert into components (value, program, initialowner) values ("XPFC", "Calendar", "spider@netscape.com");
insert into components (value, program, initialowner) values ("config", "Calendar", "spider@netscape.com");
insert into components (value, program, initialowner) values ("Core", "Calendar", "sman@netscape.com");
insert into components (value, program, initialowner) values ("NLS", "Calendar", "jusn@netscape.com");
insert into components (value, program, initialowner) values ("UI", "Calendar", "eyork@netscape.com");
insert into components (value, program, initialowner) values ("Test", "Calendar", "sman@netscape.com");
insert into components (value, program, initialowner) values ("Install", "Calendar", "sman@netscape.com");

insert into components (value, program, initialowner) values ("LDAP C SDK", "Directory", "chuckb@netscape.com");
insert into components (value, program, initialowner) values ("LDAP Java SDK", "Directory", "chuckb@netscape.com");
insert into components (value, program, initialowner) values ("PerLDAP", "Directory", "leif@netscape.com");
insert into components (value, program, initialowner) values ("LDAP Tools", "Directory", "chuckb@netscape.com");


insert into components (value, program, initialowner) values ("Macintosh FE", "Mozilla", "sdagley@netscape.com");
insert into components (value, program, initialowner) values ("Windows FE", "Mozilla", "blythe@netscape.com");
insert into components (value, program, initialowner) values ("XFE", "Mozilla", "ramiro@netscape.com");
insert into components (value, program, initialowner) values ("StubFE", "Mozilla", "toshok@netscape.com");
insert into components (value, program, initialowner) values ("Aurora/RDF FE", "Mozilla", "don@netscape.com");
insert into components (value, program, initialowner) values ("Aurora/RDF BE", "Mozilla", "guha@netscape.com");
insert into components (value, program, initialowner) values ("Berkeley DB", "Mozilla", "montulli@netscape.com");
insert into components (value, program, initialowner) values ("Browser Hooks", "Mozilla", "ebina@netscape.com");
insert into components (value, program, initialowner) values ("Build Config", "Mozilla", "briano@netscape.com");
insert into components (value, program, initialowner) values ("Composer", "Mozilla", "akkana@netscape.com");
insert into components (value, program, initialowner) values ("Compositor Library", "Mozilla", "vidur@netscape.com");
insert into components (value, program, initialowner) values ("Dialup", "Mozilla", "selmer@netscape.com");
insert into components (value, program, initialowner) values ("FontLib", "Mozilla", "dp@netscape.com");
insert into components (value, program, initialowner) values ("HTML Dialogs", "Mozilla", "nisheeth@netscape.com");
insert into components (value, program, initialowner) values ("HTML to Text/PostScript Translation", "Mozilla", "brendan@netscape.com");
insert into components (value, program, initialowner) values ("ImageLib", "Mozilla", "pnunn@netscape.com");
insert into components (value, program, initialowner) values ("JPEG Image Handling", "Mozilla", "tgl@sss.pgh.pa.us");
insert into components (value, program, initialowner) values ("PNG Image Handling", "Mozilla", "png@wco.com");
insert into components (value, program, initialowner) values ("Image Conversion Library", "Mozilla", "mjudge@netscape.com");
insert into components (value, program, initialowner) values ("I18N Library", "Mozilla", "bobj@netscape.com");
insert into components (value, program, initialowner) values ("Java Stubs", "Mozilla", "warren@netscape.com");
insert into components (value, program, initialowner) values ("JavaScript", "Mozilla", "mccabe@netscape.com");
insert into components (value, program, initialowner) values ("JavaScript Debugger", "Mozilla", "jband@netscape.com");
insert into components (value, program, initialowner) values ("JavaScript/Java Reflection", "Mozilla", "fur@netscape.com");
insert into components (value, program, initialowner) values ("Layout", "Mozilla", "toshok@netscape.com");
insert into components (value, program, initialowner) values ("LibMocha", "Mozilla", "mlm@netscape.com");
insert into components (value, program, initialowner) values ("MIMELib", "Mozilla", "terry@mozilla.org");
insert into components (value, program, initialowner) values ("NetLib", "Mozilla", "gagan@netscape.com");
insert into components (value, program, initialowner) values ("NSPR", "Mozilla", "wtc@netscape.com");
insert into components (value, program, initialowner) values ("Password Cache", "Mozilla", "montulli@netscape.com");
insert into components (value, program, initialowner) values ("PICS", "Mozilla", "montulli@netscape.com");
insert into components (value, program, initialowner) values ("Plugins", "Mozilla", "amusil@netscape.com");
insert into components (value, program, initialowner) values ("Preferences", "Mozilla", "aoki@netscape.com");
insert into components (value, program, initialowner) values ("Progress Window", "Mozilla", "atotic@netscape.com");
insert into components (value, program, initialowner) values ("Registry", "Mozilla", "dveditz@netscape.com");
insert into components (value, program, initialowner) values ("Scheduler", "Mozilla", "aoki@netscape.com");
insert into components (value, program, initialowner) values ("Security Stubs", "Mozilla", "jsw@netscape.com");
insert into components (value, program, initialowner) values ("SmartUpdate", "Mozilla", "dveditz@netscape.com");
insert into components (value, program, initialowner) values ("XML", "Mozilla", "guha@netscape.com");
insert into components (value, program, initialowner) values ("XP-COM", "Mozilla", "scullin@netscape.com");
insert into components (value, program, initialowner) values ("XP File Handling", "Mozilla", "atotic@netscape.com");
insert into components (value, program, initialowner) values ("XP Miscellany", "Mozilla", "brendan@netscape.com");
insert into components (value, program, initialowner) values ("XP Utilities", "Mozilla", "toshok@netscape.com");
insert into components (value, program, initialowner) values ("Zlib", "Mozilla", "pnunn@netscape.com");



insert into components (value, program, initialowner) values ("Platform: Lesstif on Linux", "Mozilla", "ramiro@netscape.com");
insert into components (value, program, initialowner) values ("Platform: OS/2", "Mozilla", "law@netscape.com");
insert into components (value, program, initialowner) values ("Platform: MacOS/PPC", "Mozilla", "sdagley@netscape.com");
insert into components (value, program, initialowner) values ("Platform: Rhapsody", "Mozilla", "mcafee@netscape.com");
insert into components (value, program, initialowner) values ("Platform: MFC/Win32 on Windows", "Mozilla", "blythe@netscape.com");


insert into components (value, program, initialowner) values ("ActiveX Wrapper", "NGLayout", "locka@iol.ie");
insert into components (value, program, initialowner) values ("Content Model", "NGLayout", "kipp@netscape.com");
insert into components (value, program, initialowner) values ("Cookies", "NGLayout", "scullin@netscape.com");
insert into components (value, program, initialowner) values ("DOM", "NGLayout", "vidur@netscape.com");
insert into components (value, program, initialowner) values ("DOM", "NGLayout", "vidur@netscape.com");
insert into components (value, program, initialowner) values ("Editing Interfaces", "NGLayout", "kostello@netscape.com");
insert into components (value, program, initialowner) values ("Embedding APIs", "NGLayout", "jevering@netscape.com");
insert into components (value, program, initialowner) values ("Event Handling", "NGLayout", "joki@netscape.com");
insert into components (value, program, initialowner) values ("Event Handling", "NGLayout", "joki@netscape.com");
insert into components (value, program, initialowner) values ("Form Submission", "NGLayout", "pollmann@netscape.com");
insert into components (value, program, initialowner) values ("HTMLFrames", "NGLayout", "karnaze@netscape.com");
insert into components (value, program, initialowner) values ("HTMLTables", "NGLayout", "karnaze@netscape.com");
insert into components (value, program, initialowner) values ("Layout", "NGLayout", "troy@netscape.com");
insert into components (value, program, initialowner) values ("Networking Library", "NGLayout", "gagan@netscape.com");
insert into components (value, program, initialowner) values ("Parser", "NGLayout", "rickg@netscape.com");
insert into components (value, program, initialowner) values ("Plug-ins", "NGLayout", "amusil@netscape.com");
insert into components (value, program, initialowner) values ("Rendering", "NGLayout", "michaelp@netscape.com");
insert into components (value, program, initialowner) values ("Selection and Search", "NGLayout", "kostello@netscape.com");
insert into components (value, program, initialowner) values ("Style System", "NGLayout", "peterl@netscape.com");
insert into components (value, program, initialowner) values ("Threading", "NGLayout", "rpotts@netscape.com");
insert into components (value, program, initialowner) values ("Viewer App", "NGLayout", "rickg@netscape.com");
insert into components (value, program, initialowner) values ("Widget Set", "NGLayout", "kmcclusk@netscape.com");
insert into components (value, program, initialowner) values ("XPCOM", "NGLayout", "scullin@netscape.com");
insert into components (value, program, initialowner) values ("xpidl", "NGLayout", "shaver@netscape.com");


insert into components (value, program, initialowner) values ("Bonsai", "Webtools", "terry@mozilla.org");
insert into components (value, program, initialowner) values ("Bugzilla", "Webtools", "terry@mozilla.org");
insert into components (value, program, initialowner) values ("Despot", "Webtools", "terry@mozilla.org");
insert into components (value, program, initialowner) values ("Mozbot", "Webtools", "harrison@netscape.com");
insert into components (value, program, initialowner) values ("Tinderbox", "Webtools", "terry@mozilla.org");


select * from components;

show columns from components;
show index from components;

OK_ALL_DONE
