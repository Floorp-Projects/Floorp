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




insert into components (value, program, initialowner) values ("Macintosh FE", "Communicator", "sdagley@netscape.com");
insert into components (value, program, initialowner) values ("Windows FE", "Communicator", "blythe@netscape.com");
insert into components (value, program, initialowner) values ("XFE", "Communicator", "ramiro@netscape.com");
insert into components (value, program, initialowner) values ("StubFE", "Communicator", "toshok@netscape.com");
insert into components (value, program, initialowner) values ("Aurora/RDF FE", "Communicator", "don@netscape.com");
insert into components (value, program, initialowner) values ("Aurora/RDF BE", "Communicator", "guha@netscape.com");
insert into components (value, program, initialowner) values ("Berkeley DB", "Communicator", "montulli@netscape.com");
insert into components (value, program, initialowner) values ("Browser Hooks", "Communicator", "ebina@netscape.com");
insert into components (value, program, initialowner) values ("Build Config", "Communicator", "briano@netscape.com");
insert into components (value, program, initialowner) values ("Composer", "Communicator", "brade@netscape.com");
insert into components (value, program, initialowner) values ("Compositor Library", "Communicator", "vidur@netscape.com");
insert into components (value, program, initialowner) values ("Dialup", "Communicator", "selmer@netscape.com");
insert into components (value, program, initialowner) values ("FontLib", "Communicator", "dp@netscape.com");
insert into components (value, program, initialowner) values ("HTML Dialogs", "Communicator", "nisheeth@netscape.com");
insert into components (value, program, initialowner) values ("HTML to Text/PostScript Translation", "Communicator", "brendan@netscape.com");
insert into components (value, program, initialowner) values ("ImageLib", "Communicator", "pnunn@netscape.com");
insert into components (value, program, initialowner) values ("JPEG Image Handling", "Communicator", "tgl@sss.pgh.pa.us");
insert into components (value, program, initialowner) values ("PNG Image Handling", "Communicator", "png@wco.com");
insert into components (value, program, initialowner) values ("Image Conversion Library", "Communicator", "mjudge@netscape.com");
insert into components (value, program, initialowner) values ("I18N Library", "Communicator", "bobj@netscape.com");
insert into components (value, program, initialowner) values ("Java Stubs", "Communicator", "warren@netscape.com");
insert into components (value, program, initialowner) values ("JavaScript", "Communicator", "mccabe@netscape.com");
insert into components (value, program, initialowner) values ("JavaScript Debugger", "Communicator", "jband@netscape.com");
insert into components (value, program, initialowner) values ("JavaScript/Java Reflection", "Communicator", "fur@netscape.com");
insert into components (value, program, initialowner) values ("Layout", "Communicator", "djw@netscape.com");
insert into components (value, program, initialowner) values ("LibMocha", "Communicator", "chouck@netscape.com");
insert into components (value, program, initialowner) values ("MIMELib", "Communicator", "terry@netscape.com");
insert into components (value, program, initialowner) values ("NetLib", "Communicator", "gagan@netscape.com");
insert into components (value, program, initialowner) values ("NSPR", "Communicator", "wtc@netscape.com");
insert into components (value, program, initialowner) values ("Password Cache", "Communicator", "montulli@netscape.com");
insert into components (value, program, initialowner) values ("PICS", "Communicator", "montulli@netscape.com");
insert into components (value, program, initialowner) values ("Plugins", "Communicator", "byrd@netscape.com");
insert into components (value, program, initialowner) values ("Preferences", "Communicator", "aoki@netscape.com");
insert into components (value, program, initialowner) values ("Progress Window", "Communicator", "atotic@netscape.com");
insert into components (value, program, initialowner) values ("Registry", "Communicator", "dveditz@netscape.com");
insert into components (value, program, initialowner) values ("Scheduler", "Communicator", "aoki@netscape.com");
insert into components (value, program, initialowner) values ("Security Stubs", "Communicator", "jsw@netscape.com");
insert into components (value, program, initialowner) values ("SmartUpdate", "Communicator", "dveditz@netscape.com");
insert into components (value, program, initialowner) values ("XML", "Communicator", "guha@netscape.com");
insert into components (value, program, initialowner) values ("XP-COM", "Communicator", "scullin@netscape.com");
insert into components (value, program, initialowner) values ("XP File Handling", "Communicator", "atotic@netscape.com");
insert into components (value, program, initialowner) values ("XP Miscellany", "Communicator", "brendan@netscape.com");
insert into components (value, program, initialowner) values ("XP Utilities", "Communicator", "toshok@netscape.com");
insert into components (value, program, initialowner) values ("Zlib", "Communicator", "pnunn@netscape.com");



insert into components (value, program, initialowner) values ("Platform: Lesstif on Linux", "Communicator", "ramiro@netscape.com");
insert into components (value, program, initialowner) values ("Platform: OS/2", "Communicator", "law@netscape.com");
insert into components (value, program, initialowner) values ("Platform: MacOS/PPC", "Communicator", "sdagley@netscape.com");
insert into components (value, program, initialowner) values ("Platform: Rhapsody", "Communicator", "mcafee@netscape.com");
insert into components (value, program, initialowner) values ("Platform: MFC/Win32 on Windows", "Communicator", "blythe@netscape.com");


insert into components (value, program, initialowner) values ("UI", "Bugzilla", "terry@netscape.com");
insert into components (value, program, initialowner) values ("Database", "Bugzilla", "terry@netscape.com");


select * from components;

show columns from components;
show index from components;

OK_ALL_DONE
