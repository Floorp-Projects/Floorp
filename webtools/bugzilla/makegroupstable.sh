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

drop table groups
OK_ALL_DONE

mysql << OK_ALL_DONE
use bugs;
create table groups (
    # This must be a power of two.  Groups are identified by a bit; sets of
    # groups are indicated by or-ing these values together.
    bit bigint not null,
    name varchar(255) not null,
    description text not null,

    # isbuggroup is nonzero if this is a group that controls access to a set
    # of bugs.  In otherword, the groupset field in the bugs table should only
    # have this group's bit set if isbuggroup is nonzero.
    isbuggroup tinyint not null,

    # User regexp is which email addresses are initially put into this group.
    # This is only used when an email account is created; otherwise, profiles
    # may be individually tweaked to add them in and out of groups.
    userregexp tinytext not null,


    unique(bit),
    unique(name)

);


insert into groups (bit, name, description, userregexp) values (pow(2,0), "tweakparams", "Can tweak operating parameters", "");
insert into groups (bit, name, description, userregexp) values (pow(2,1), "editgroupmembers", "Can put people in and out of groups that they are members of.", "");
insert into groups (bit, name, description, userregexp) values (pow(2,2), "creategroups", "Can create and destroy groups.", "");
insert into groups (bit, name, description, userregexp) values (pow(2,3), "editcomponents", "Can create, destroy, and edit components.", "");



show columns from groups;
show index from groups;


# Check for bad bit values.

select "*** Bad bit value", bit from groups where bit != pow(2, round(log(bit) / log(2)));

OK_ALL_DONE
