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




insert into components (value, program, description) values ("TestComponent", "TestProduct", "This is a test component in the test product database.  This ought to be blown away and replaced with real stuff in a finished installation of bugzilla.");

show columns from components;
show index from components;

OK_ALL_DONE
