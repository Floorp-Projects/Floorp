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

drop table bugs_activity
OK_ALL_DONE
mysql << OK_ALL_DONE
use bugs;
create table bugs_activity (
        bug_id mediumint not null,
        who mediumint not null,
        when datetime not null,
        field varchar(64) not null,
        oldvalue tinytext,
        newvalue tinytext,

        index (bug_id),
        index (when),
	index (field)
);



show columns from bugs_activity;
show index from bugs_activity;

OK_ALL_DONE
