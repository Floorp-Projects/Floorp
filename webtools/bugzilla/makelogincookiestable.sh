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

mysql bugs > /dev/null 2>/dev/null << OK_ALL_DONE

drop table logincookies;
OK_ALL_DONE

mysql bugs << OK_ALL_DONE

create table logincookies (
    cookie mediumint not null auto_increment primary key,
    userid mediumint not null,
    cryptpassword varchar(64),
    hostname varchar(128),
    lastused timestamp,
    index(lastused)
);

show columns from logincookies;
show index from logincookies;

OK_ALL_DONE
