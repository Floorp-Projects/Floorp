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

drop table products;
OK_ALL_DONE

mysql << OK_ALL_DONE
use bugs;
create table products (
product tinytext,
description mediumtext,
milestoneurl tinytext not null,
disallownew tinyint not null
);



insert into products (product, description, milestoneurl) values ("Browser", 'For bugs about the Mozilla Browser, including bugs with the <a href="http://www.mozilla.org/newlayout/">New Layout</a> project', 'http://www.mozilla.org/projects/seamonkey/milestones');
insert into products (product, description) values ("Calendar", 'For bugs about the <a href="http://www.mozilla.org/projects/calendar">Calendar</a> project');
insert into products (product, description) values ("CCK", 'For bugs about the <a href="http://www.mozilla.org/projects/cck">Client Customization Kit<a> project');
insert into products (product, description) values ("Directory", 'For bugs about the <a href="http://www.mozilla.org/directory">Directory (LDAP)</a> project');
insert into products (product, description) values ("Grendel", 'For bugs about the <a href="http://www.mozilla.org/projects/grendel/">Grendel</a> java-based mail/news reader');
insert into products (product, description) values ("MailNews", 'For bugs about the <a href="http://www.mozilla.org/mailnews/index.html">Mozilla Mail/News</a> project');
insert into products (product, description, disallownew) values ("MozillaClassic", "For bugs about the Mozilla web browser", 1);
insert into products (product, description) values ("Webtools", 'For bugs about the web-based tools that mozilla.org uses.  This include Bugzilla (problems you are having with this bug system itself), <a href="http://www.mozilla.org/bonsai.html">Bonsai</a>, and <a href="http://www.mozilla.org/tinderbox.html">Tinderbox</a>.');

