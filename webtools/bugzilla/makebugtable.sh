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

drop table bugs;
OK_ALL_DONE

mysql << OK_ALL_DONE
use bugs;
create table bugs (
bug_id mediumint not null auto_increment primary key,
assigned_to mediumint not null, # This is a comment.
bug_file_loc text,
bug_severity enum("critical", "major", "normal", "minor", "trivial", "enhancement") not null,
bug_status enum("NEW", "ASSIGNED", "REOPENED", "RESOLVED", "VERIFIED", "CLOSED") not null,
creation_ts datetime,
delta_ts timestamp,
short_desc mediumtext,
long_desc mediumtext,
op_sys enum("All", "Windows 3.1", "Windows 95", "Windows 98", "Windows NT", "Mac System 7", "Mac System 7.5", "Mac System 7.1.6", "Mac System 8.0", "Mac System 8.5", "AIX", "BSDI", "HP-UX", "IRIX", "Linux", "OSF/1", "Solaris", "SunOS", "OS/2", "other") not null,
priority enum("P1", "P2", "P3", "P4", "P5") not null,
product varchar(16) not null,
rep_platform enum("All", "DEC", "HP", "Macintosh", "PC", "SGI", "Sun", "Other"),
reporter mediumint not null,
version varchar(16) not null,
area enum("BUILD", "CODE", "CONTENT", "DOC", "PERFORMANCE", "TEST", "UI", "i18n", "l10n") not null,
component varchar(50) not null,
resolution enum("", "FIXED", "INVALID", "WONTFIX", "LATER", "REMIND", "DUPLICATE", "WORKSFORME") not null,
target_milestone varchar(20) not null,
qa_contact mediumint not null,
status_whiteboard mediumtext not null,

index (assigned_to),
index (delta_ts),
index (bug_severity),
index (bug_status),
index (op_sys),
index (priority),
index (product),
index (reporter),
index (version),
index (area),
index (component),
index (resolution),
index (target_milestone),
index (qa_contact)

);

show columns from bugs;
show index from bugs;


OK_ALL_DONE
