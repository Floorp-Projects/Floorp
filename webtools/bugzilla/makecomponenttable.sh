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
initialqacontact tinytext not null, # Should arguably be a mediumint!
description mediumtext not null
);




insert into components (value, program, initialowner, initialqacontact, description) values ("ActiveX Wrapper", "Browser", "locka@iol.ie", "cpratt@netscape.com", "This is the active-x wrapper that is used when people want to embed gecko in their application on windows. This is an external developer's code to wrap gecko up as an activeX control to replace IE as the embedded HTML control.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Apprunner", "Browser", "don@netscape.com", "leger@netscape.com", "For bugs against the apprunner application which include: launch/exit crashes, features missing, and apprunner parity across platforms.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Autofill", "Browser", "morse@netscape.com", "junruh@netscape.com", "Bugs against automatic fill of personal data.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Build Config", "Browser", "briano@netscape.com", "cyeh@netscape.com", "This component is for bugs against build problems.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Compositor", "Browser", "michaelp@netscape.com", "petersen@netscape.com", "Compositor (aka rendering) is like layout, a catch-all, but for painting problems. If the object is in the right place, but doesn't paint correctly, it's a rendering bug. If it paints ok, but in the wrong place, it's a layout bug. Rendering is for <i>drawing</i>.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Cookies", "Browser", "morse@netscape.com", "paulmac@netscape.com", "A general mechanism which server side connections (such as CGI scripts) can use to both store and retrieve information on the client side of the connection. This refers to HTML cookies; little blobs of data we store and share with sites");
insert into components(value, program, initialowner, initialqacontact, description) values("DOM Level 0", "Browser", "vidur@netscape.com", "pdesale@netscape.com", "This component consists of all functionality related to JavaScript manipulation of window, document and navigator components that was available in Navigator 4.x and before.");
insert into components(value, program, initialowner, initialqacontact, description) values("DOM Level 1", "Browser", "vidur@netscape.com", "gerardok@netscape.com", "This component consists of all functionality related to JavaScript or XPCOM-based access and manipulation of a document and its components using the interfaces <a href='http://www.w3.org/TR/REC-DOM-Level-1'>defined by the W3C</a>.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Editor", "Browser", "kostello@netscape.com", "sujay@netscape.com", "An embedable editing object within the browser and the mail editing application");
insert into components (value, program, initialowner, initialqacontact, description) values ("Embedding APIs", "Browser", "nisheeth@netscape.com", "claudius@netscape.com", "The embedding API is the set of functions that an external application uses to host an instance of the layout engine within itself.  The embedded layout engine provides services for rendering web content (HTML, XML, CSS, etc).  For example, Encyclopedia Brittanica currently embeds the IE 4.0 layout engine within itself and uses it to show its reference pages.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Event Handling", "Browser", "joki@netscape.com", "janc@netscape.com", "Any strangeness with keyboard typing, mouse actions, focus changes.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Form Submission", "Browser", "karnaze@netscape.com", "cpratt@netscape.com", "This is HTML forms -- layout, rendering, functionality and submission.");
insert into components (value, program, initialowner, initialqacontact, description) values ("HTML Dialogs", "Browser", "davidm@netscape.com", "cpratt@netscape.com", "For bugs in dialog windows and alert windows.");
insert into components (value, program, initialowner, initialqacontact, description) values ("HTMLFrames", "Browser", "karnaze@netscape.com", "glynn@netscape.com", "This is HTML framesets -- layout and rendering. HTMLFrames == html frame/frameset/iframe tag handling.");
insert into components (value, program, initialowner, initialqacontact, description) values ("HTMLTables", "Browser", "karnaze@netscape.com", "chrisd@netscape.com", "This is HTML Tables -- layout and rendering. Editing is applicable to most of the layout related categories, both Browser and Ender would use this component.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Image Conversion Library", "Browser", "mjudge@netscape.com", "elig@netscape.com", "Encoding rather than decoding. Image handling in editor.");
insert into components (value, program, initialowner, initialqacontact, description) values ("ImageLib", "Browser", "pnunn@netscape.com", "elig@netscape.com", " The functions that enable images. Decoding rather than encoding image library. This code is responsible for presenting different image formats (png, jpeg, etc.) in a consistant manner to layout.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Internationalization", "Browser", "ftang@netscape.com", "teruko@netscape.com", "Internationalization is the process of designing and developing a software product to function in multiple locales. This process involves identifying the locales that must be supported, designing features which support those locales, and writing code that functions equally well in any of the supported locales.");
insert into components (value, program, initialowner, initialqacontact, description) values ("JavaScript Debugger", "Browser", "jband@netscape.com", "cbegle@netscape.com", "Bugs, problems, enhancements for the javascript debugger tool.");
insert into components (value, program, initialowner, initialqacontact, description) values ("JavaScript", "Browser", "norris@netscape.com", "cbegle@netscape.com", "Core JavaScript interpreter, JS 1.4");
insert into components (value, program, initialowner, initialqacontact, description) values ("Layout", "Browser", "troy@netscape.com", "petersen@netscape.com", "Layout is for general problems with the positioning of objects on the page. If you can't tell specifically why an object seems to be in the wrong place, use layout.");
insert into components (value, program, initialowner, initialqacontact, description) values ("libPref", "Browser", "hshaw@netscape.com", "paulmac@netscape.com", "Backend library for implementing XP preferences");
insert into components (value, program, initialowner, initialqacontact, description) values ("Live Connect", "Browser", "fur@netscape.com", "cbegle@netscape.com", "<need to get definition>");
insert into components (value, program, initialowner, initialqacontact, description) values ("Localization", "Browser", "rchen@netscape.com", "teruko@netscape.com", "Localization is the process of adapting software for a specific international market; this process includes translating the user interface, resizing dialog boxes, replacing icons and other culturally sensitive graphics (if necessary), customizing features (if necessary), and testing the localized product to ensure that the program still works.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Mac Install Wizard", "Browser", "sgehani@netscape.com", "gbush@netscape.com", "Front end Mac installer problems; both functional and UI.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Networking Library", "Browser", "gagan@netscape.com", "paulmac@netscape.com", "Backend network library offers protocol support for reading net data.  (HTTP, MIME, etc.)");
insert into components (value, program, initialowner, initialqacontact, description) values ("NSPR", "Browser", "wtc@netscape.com", "phillip@netscape.com", "Netscape Portable Runtime library. libc and threading abstraction.");
insert into components (value, program, initialowner, initialqacontact, description) values ("OJI", "Browser", "amusil@netscape.com", "glynn@netscape.com", "Pluggable java API");
insert into components (value, program, initialowner, initialqacontact, description) values ("Parser", "Browser", "rickg@netscape.com", "janc@netscape.com", "This system consumes content  from the web, parses, validates and builds a content model (document)");
insert into components (value, program, initialowner, initialqacontact, description) values ("Password Cache", "Browser", "morse@netscape.com", "paulmac@netscape.com", "Bugs against password related problems.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Plug-ins", "Browser", "amusil@netscape.com", "glynn@netscape.com", "Plugins allow 3rd parties to register a  binary library to be called when a given mime datatype is encountered. The plugin can layout and render the datatype.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Pref UI", "Browser", "shuang@netscape.com", "paulmac@netscape.com", "<need to get definition>");
insert into components (value, program, initialowner, initialqacontact, description) values ("Profile Manager", "Browser", "dbragg@netscape.com", "paulmac@netscape.com", "<need to get definition>");
insert into components (value, program, initialowner, initialqacontact, description) values ("Profile Migration", "Browser", "dbragg@netscape.com", "gbush@netscape.com", "Problems with conversion from 4.x to 5.x profiles");
insert into components (value, program, initialowner, initialqacontact, description) values ("Progress Window", "Browser", "evaughan@netscape.com", "cpratt@netscape.com", "Download status window problems");
insert into components (value, program, initialowner, initialqacontact) values ("RDF", "Browser", "waterson@netscape.com", "phillip@netscape.com");
insert into components (value, program, initialowner, initialqacontact, description) values ("Security", "Browser", "tomw@netscape.com", "dshea@netscape.com", "<need to get definition>");
insert into components (value, program, initialowner, initialqacontact, description) values ("Selection and Search", "Browser", "mjudge@netscape.com", "claudius@netscape.com", "Selection refers to the user action of selecting all or part of a document and highlighting the selected content. Search refers to finding content within the larger context of a document.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Silent Download", "Browser", "dougt@netscape.com", "gbush@netscape.com", "Background bits download, and notification for new release available for install.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Single Signon", "Browser", "morse@netscape.com", "junruh@netscape.com", "<need to get definition>");
insert into components (value, program, initialowner, initialqacontact, description) values ("Style System", "Browser", "peterl@netscape.com", "chrisd@netscape.com", "CSS, XSL - CSS and HTML attribute handling. There's some overlap here with the parser, if you're not sure just guess.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Threading", "Browser", "rpotts@netscape.com", "rpotts@netscape.com", "Unlikely a tester would be able to tell there was a threading problem specifically. The result of threading problems are generally lock-ups. But unless you have a stack trace that specifically points the finger at threading, you should submit the bug against another component.");
insert into components (value, program, initialowner, initialqacontact, description) values ("UE/UI", "Browser", "shuang@netscape.com", "", "User Experience.  User interface, useability, something's just too hard to figure out, etc.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Viewer App", "Browser", "rickg@netscape.com", "leger@netscape.com", "This is the viewer test app. Utilized by NG Layout engineers to test the layout engine.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Widget Set", "Browser", "karnaze@netscape.com", "phillip@netscape.com", "Refers to mozilla/widget, the XP widget library used by the layout engine.  Think FORM elements: buttons, edit fields, etc. ");
insert into components (value, program, initialowner, initialqacontact, description) values ("Windows Install Wizard", "Browser", "ssu@netscape.com", "gbush@netscape.com", "Front end Win32 installer problems; both functional and UI.");
insert into components (value, program, initialowner, initialqacontact) values ("XML", "Browser", "guha@netscape.com", "beppe@netscape.com");
insert into components (value, program, initialowner, initialqacontact) values ("XP File Handling", "Browser", "mcmullen@netscape.com", "mcmullen@netscape.com");
insert into components (value, program, initialowner, initialqacontact) values ("XP Miscellany", "Browser", "brendan@netscape.com", "brendan@netscape.com");
insert into components (value, program, initialowner, initialqacontact, description) values ("XP Toolkit/Widgets", "Browser", "trudelle@netscape.com", "trudelle@netscape.com", "Cross-platform library of UI widgest like buttons, dialogs, toolbars, tree control, etc.");
insert into components (value, program, initialowner, initialqacontact) values ("XP Utilities", "Browser", "rickg@netscape.com", "rickg@netscape.com");
insert into components (value, program, initialowner, initialqacontact, description) values ("XPApps", "Browser", "don@netscape.com", "beppe@netscape.com", "Component/service applications for XP support.");
insert into components (value, program, initialowner, initialqacontact, description) values ("XPCOM Registry", "Browser", "dp@netscape.com", "dp@netscape.com", "<need to get definition>");
insert into components (value, program, initialowner, initialqacontact, description) values ("XPCOM", "Browser", "scc@netscape.com", "scc@netscape.com", "This is the basis of our component technology; this covers the mozilla/xpcom source directory and includes the \"repository\". Unlikely a tester would be able to tell there was an XPCOM problem specifically. Unless you have a stack trace that specifically points the finger at threading, you should submit the bug against another component.");
insert into components (value, program, initialowner, initialqacontact, description) values ("xpidl", "Browser", "vidur@netscape.com", "vidur@netscape.com", "The tool that translates idl into xpcom header files and java script stubs.");
insert into components (value, program, initialowner, initialqacontact, description) values ("XPInstall", "Browser", "dveditz@netscape.com", "gbush@netscape.com", "All SmartUpdate bugs.  Failure on JAR installation process.");
insert into components (value, program, initialowner, initialqacontact, description) values ("XUL", "Browser", "danm@netscape.com", "danm@netscape.com", "Cross platform language for specifying the browser UI");
insert into components (value, program, initialowner, initialqacontact, description) values ("Zlib", "Browser", "dp@netscape.com", "elig@netscape.com", "Compression library.");


insert into components (value, program, initialowner, initialqacontact) values ("XPFC", "Calendar", "spider@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("config", "Calendar", "spider@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Core", "Calendar", "sman@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("NLS", "Calendar", "jusn@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("UI", "Calendar", "eyork@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Test", "Calendar", "sman@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Install", "Calendar", "sman@netscape.com", "");

insert into components (value, program, initialowner, initialqacontact, description) values ("CCK-Wizard", "CCK", "selmer@netscape.com", "bmartin@netscape.com", "The CCK wizard allows the user to gather data by walking through a series of screens to create a set of customizations and build a media image.");
insert into components (value, program, initialowner, initialqacontact, description) values ("CCK-Installation", "CCK", "selmer@netscape.com", "bmartin@netscape.com", "The CCK installation includes verifying the CD-layout. Other areas include the installations screens, directory structure, branding and launching the CCK Wizard.");
insert into components (value, program, initialowner, initialqacontact, description) values ("CCK-Shell", "CCK", "selmer@netscape.com", "bmartin@netscape.com", "CCK allows an ISP to customize the first screen seen by the user. The shell dialog(s) displays a background image as well as buttons and text explaining the installation options to the user.");
insert into components (value, program, initialowner, initialqacontact, description) values ("CCK-Whitebox", "CCK", "selmer@netscape.com", "bmartin@netscape.com", "This is a series of whitebox tests to test the critical functions of the CCK Wizard.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Dialup-Install", "CCK", "selmer@netscape.com", "bmartin@netscape.com", "This is the Installation of a custom build which includes the Account Setup module created by CCK");
insert into components (value, program, initialowner, initialqacontact, description) values ("Dialup-Account Setup", "CCK", "selmer@netscape.com", "bmartin@netscape.com", "The Account Setup Wizard is used by the end users who want to create or modify an existing Internet account.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Dialup-Mup/Muc", "CCK", "selmer@netscape.com", "bmartin@netscape.com", "This refers to multiple user configurations which allows association of Microsoft dialup networking connectoids to Netscape user profiles.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Dialup-Upgrade", "CCK", "selmer@netscape.com", "bmartin@netscape.com", "This is the upgrade path from 4.5 to the latest release where the user preferences are preserved.");
insert into components (value, program, initialowner, initialqacontact, description) values ("AS-Whitebox", "CCK", "selmer@netscape.com", "bmartin@netscape.com", "This is a series of whitebox tests to test the critical functions of the Account Setup.");

insert into components (value, program, initialowner, initialqacontact) values ("LDAP C SDK", "Directory", "chuckb@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("LDAP Java SDK", "Directory", "chuckb@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("PerLDAP", "Directory", "leif@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("LDAP Tools", "Directory", "chuckb@netscape.com", "");

insert into components (value, program, initialowner) values ("Front end", "Grendel", "grail@cafebabe.org");
insert into components (value, program, initialowner) values ("IMAP", "Grendel", "edwin@woudt.nl");
insert into components (value, program, initialowner) values ("POP", "Grendel", "edwin@woudt.nl");
insert into components (value, program, initialowner) values ("SMTP", "Grendel", "edwin@woudt.nl");
insert into components (value, program, initialowner) values ("NNTP", "Grendel", "edwin@woudt.nl");
insert into components (value, program, initialowner) values ("Address Book", "Grendel", "talisman@anamorphic.com");


insert into components (value, program, initialowner, initialqacontact, description) values ("Networking", "MailNews", "mscott@netscape.com", "lchiang@netscape.com", "Integration with libnet, protocol support for POP3, IMAP4, SMTP, NNTP and LDAP");
insert into components (value, program, initialowner, initialqacontact, description) values ("Database", "MailNews", "davidmc@netscape.com", "lchiang@netscape.com", "Persistent storage of address books and mail/news summary files");
insert into components (value, program, initialowner, initialqacontact, description) values ("MIME", "MailNews", "rhp@netscape.com", "lchiang@netscape.com", "Parsing the MIME structure");
insert into components (value, program, initialowner, initialqacontact, description) values ("Security", "MailNews", "jefft@netscape.com", "lchiang@netscape.com", "SSL, S/MIME for mail");
insert into components (value, program, initialowner, initialqacontact, description) values ("Composition", "MailNews", "ducarroz@netscape.com", "lchiang@netscape.com", "Front-end and back-end of message composition and sending");
insert into components (value, program, initialowner, initialqacontact, description) values ("Address Book", "MailNews", "putterman@netscape.com", "lchiang@netscape.com", "Names, email addresses, phone numbers, etc.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Front End", "MailNews", "phil@netscape.com", "lchiang@netscape.com", "Three pane view, sidebar contents, toolbars, dialogs, etc.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Back End", "MailNews", "phil@netscape.com", "lchiang@netscape.com", "RDF data sources and application logic for local mail, news, IMAP and LDAP");
insert into components (value, program, initialowner, initialqacontact, description) values ("XPInstall", "MailNews", "dveditz@netscape.com", "gbush@netscape.com", "All SmartUpdate bugs.  Failure on JAR installation process.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Silent Download", "MailNews", "dougt@netscape.com", "gbush@netscape.com", "Background bits download, and notification for new release available for install.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Windows Install Wizard", "MailNews", "ssu@netscape.com", "gbush@netscape.com", "Front end Win32 installer problems; both functional and UI.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Mac Install Wizard", "MailNews", "sgehani@netscape.com", "gbush@netscape.com", "Front end Mac installer problems; both functional and UI.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Profile Migration", "MailNews", "dbragg@netscape.com", "gbush@netscape.com", "Problems with conversion from 4.x to 5.x profiles");
insert into components (value, program, initialowner, initialqacontact, description) values ("Internationalization", "MailNews", "nhotta@netscape.com", "momoi@netscape.com", "Internationalization is the process of designing and developing a software product to function in multiple locales. This process involves identifying the locales that must be supported, designing features which support those locales, and writing code that functions equally well in any of the supported locales.");
insert into components (value, program, initialowner, initialqacontact, description) values ("Localization", "MailNews", "rchen@netscape.com", "momoi@netscape.com", "Localization is the process of adapting software for a specific international market; this process includes translating the user interface, resizing dialog boxes, replacing icons and other culturally sensitive graphics (if necessary), customizing features (if necessary), and testing the localized product to ensure that the program still works.");



insert into components (value, program, initialowner, initialqacontact) values ("Macintosh FE", "MozillaClassic", "sdagley@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Windows FE", "MozillaClassic", "blythe@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("XFE", "MozillaClassic", "ramiro@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("StubFE", "MozillaClassic", "rickg@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Aurora/RDF FE", "MozillaClassic", "don@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Aurora/RDF BE", "MozillaClassic", "guha@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Berkeley DB", "MozillaClassic", "montulli@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Browser Hooks", "MozillaClassic", "ebina@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Composer", "MozillaClassic", "akkana@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Compositor Library", "MozillaClassic", "vidur@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("FontLib", "MozillaClassic", "dp@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("HTML to Text/PostScript Translation", "MozillaClassic", "brendan@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("JPEG Image Handling", "MozillaClassic", "tgl@sss.pgh.pa.us", "");
insert into components (value, program, initialowner, initialqacontact) values ("PNG Image Handling", "MozillaClassic", "png@wco.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Java Stubs", "MozillaClassic", "warren@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact) values ("Layout", "MozillaClassic", "rickg@netscape.com", "petersen@netscape.com");
insert into components (value, program, initialowner, initialqacontact) values ("LibMocha", "MozillaClassic", "mlm@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact, description) values ("MIMELib", "MozillaClassic", "terry@mozilla.org", "lchiang@netscape.com", "Bugs in this area now assigned to MIME component in Mail/News product.");
insert into components (value, program, initialowner, initialqacontact, description) values ("NetLib", "MozillaClassic", "gagan@netscape.com", "paulmac@netscape.com", "Bugs in this area now assigned to Network Library component in Browser product.");
insert into components (value, program, initialowner, initialqacontact) values ("PICS", "MozillaClassic", "montulli@netscape.com", "");
insert into components (value, program, initialowner, initialqacontact, description) values ("Preferences", "MozillaClassic", "mcmullen@netscape.com", "beppe@netscape.com", "Please see libPref, Pref UI or Profile Mgr components under Browser product for bugs against Preferences.");
insert into components (value, program, initialowner, initialqacontact) values ("Registry", "MozillaClassic", "law@netscape.com", "bond@netscape.com");
insert into components (value, program, initialowner, initialqacontact) values ("Security Stubs", "MozillaClassic", "jsw@netscape.com", "dshea@netscape.com");



insert into components (value, program, initialowner, initialqacontact, description) values ("Bonsai", "Webtools", "terry@mozilla.org", "", 'Web based <a href="http://www.mozilla.org/bonsai.html">Tree control system</a> for watching the up-to-the-minute goings-on in a CVS repository');
insert into components (value, program, initialowner, initialqacontact, description) values ("Bugzilla", "Webtools", "terry@mozilla.org", "", "Use this component to report bugs in the bug system itself");
insert into components (value, program, initialowner, initialqacontact, description) values ("Despot", "Webtools", "terry@mozilla.org", "", "mozilla.org's <a href=http://cvs-mirror.mozilla.org/webtools/despot.cgi>account management</a> system");
insert into components (value, program, initialowner, initialqacontact, description) values ("LXR", "Webtools", "endico@mozilla.org", "", 'Source code <a href="http://cvs-mirror.mozilla.org/webtools/lxr/">cross reference</a> system');
insert into components (value, program, initialowner, initialqacontact, description) values ("Mozbot", "Webtools", "terry@mozilla.org", "", 'IRC robot that watches tinderbox and other things');
insert into components (value, program, initialowner, initialqacontact, description) values ("Tinderbox", "Webtools", "terry@mozilla.org", "", 'Tree-watching system to monitor <a href="http://www.mozilla.org/tinderbox.html">continuous builds</a> that we run on multiple platforms.');


select * from components;

show columns from components;
show index from components;

OK_ALL_DONE
