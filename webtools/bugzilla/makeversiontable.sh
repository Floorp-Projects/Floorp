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

drop table versions
OK_ALL_DONE

mysql << OK_ALL_DONE
use bugs;
create table versions (
value tinytext,
program tinytext
);



insert into versions (value, program) values ("other", "Calendar");
insert into versions (value, program) values ("other", "Directory");
insert into versions (value, program) values ("other", "Mozilla");
insert into versions (value, program) values ("1998-03-31", "Mozilla");
insert into versions (value, program) values ("1998-04-08", "Mozilla");
insert into versions (value, program) values ("1998-04-29", "Mozilla");
insert into versions (value, program) values ("1998-06-03", "Mozilla");
insert into versions (value, program) values ("1998-07-28", "Mozilla");
insert into versions (value, program) values ("1998-09-04", "Mozilla");
insert into versions (value, program) values ("other", "NGLayout");
insert into versions (value, program) values ("other", "Webtools");

select * from versions;

show columns from versions;
show index from versions;

OK_ALL_DONE
