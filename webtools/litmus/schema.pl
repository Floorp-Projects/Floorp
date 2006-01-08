# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Litmus.
#
# The Initial Developer of the Original Code is
# the Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chris Cooper <ccooper@deadsquid.com>
#   Zach Lipton <zach@zachlipton.com>

# Litmus database schema

our $table;

$table{branches} = 
   'branch_id smallint not null primary key auto_increment,
    product_id tinyint not null,
    name varchar(64) not null,
    detect_regexp varchar(255),
    
    index(product_id),
    index(name)';

$table{build_type_lookup} = 
	'build_type_id tinyint not null primary key auto_increment,
	 name varchar(64) not null,
	 
	 index(name)';
	 
$table{exit_status_lookup} = 
	'exit_status_id tinyint not null primary key auto_increment,
	 name varchar(64) not null,
	 
	 index(name)';

$table{locale_lookup} = 
	'abbrev varchar(16) not null primary key,
	 name varchar(64) not null,
	 
	 index(name)';

$table{log_type_lookup} = 
	'log_type_id tinyint not null primary key auto_increment,
	 name varchar(64) not null,
	 
	 index(name)';

$table{opsyses} = 
	'opsys_id smallint not null primary key auto_increment,
	 platform_id smallint not null,
	 name varchar(64) not null,
	 detect_regexp varchar(255),
	 
	 index(platform_id),
	 index(name)';

$table{platforms} = 
	'platform_id smallint not null primary key auto_increment,
	 product_id tinyint not null,
	 name varchar(64) not null,
	 detect_regexp varchar(255),
	 iconpath varchar(255),
	 
	 index(product_id),
	 index(name)';

$table{products} = 
	'product_id tinyint not null primary key auto_increment,
	 name varchar(64) not null,
	 iconpath varchar(255),
	 enabled tinyint default \'1\',
	 
	 index(name),
	 index(enabled)';

$table{subgroups} = 
	'subgroup_id smallint not null primary key auto_increment,
	 testgroup_id smallint not null,
	 name varchar(64) not null,
	 sort_order smallint(6),
	 testrunner_group_id int,
	 
	 index(testgroup_id),
	 index(name),
	 index(sort_order),
	 index(testrunner_group_id)';

$table{test_format_lookup} = 
	'format_id tinyint not null primary key auto_increment,
	 name varchar(255) not null,
	 
	 index(name)';

$table{test_groups} = 
	'testgroup_id smallint not null primary key auto_increment,
	 product_id tinyint not null,
	 name varchar(64) not null,
	 expiration_days smallint not null,
	 obsolete tinyint default \'1\',
	 testrunner_plan_id int,
	 
	 index(product_id),
	 index(name),
	 index(expiration_days),
	 index(obsolete),
	 index(testrunner_plan_id)';

$table{test_result_bugs} = 
	'test_result_id int not null primary key auto_increment,
	 last_updated datetime not null,
	 submission_time datetime not null,
	 user_id int,
	 bug_id int not null,
	 
	 index(test_result_id),
	 index(last_updated),
	 index(submission_time),
	 index(user_id)';

$table{test_result_comments} = 
	'comment_id int not null primary key auto_increment,
	 test_result_id int not null,
	 last_updated datetime not null,
	 submission_time datetime not null,
	 user_id int,
	 comment text,
	 
	 index(test_result_id),
	 index(last_updated),
	 index(submission_time),
	 index(user_id)';
	 
$table{test_result_logs} = 
	'log_id int not null primary key auto_increment,
	 test_result_id int not null,
	 last_updated datetime not null,
	 submission_time datetime not null,
	 log_path varchar(255),
	 log_type_id tinyint not null default \'1\',
	 
	 index(test_result_id),
	 index(last_updated),
	 index(submission_time),
	 index(log_path),
	 index(log_type_id)';
	 
$table{test_result_status_lookup} = 
	'result_status_id smallint not null primary key auto_increment,
	 name varchar(64) not null,
	 style varchar(255) not null,
	 class_name varchar(16),
	 
	 index(name),
	 index(style),
	 index(class_name)';
	 
$table{test_results} = 
	'testresult_id int not null primary key auto_increment,
	 test_id int not null,
	 last_updated datetime,
	 submission_time datetime,
	 user_id int,
	 platform_id smallint,
	 opsys_id smallint,
	 branch_id smallint,
	 buildid varchar(45),
	 user_agent varchar(255),
	 result_id smallint,
	 build_type_id tinyint not null default \'1\',
	 machine_name varchar(64),
	 exit_status_id tinyint not null default \'1\',
	 duration_ms int unsigned,
	 talkback_id int unsigned,
	 validity_id tinyint not null default \'1\',
	 vetting_status_id tinyint not null default \'1\',
	 locale_abbrev varchar(16) not null default \'en-US\',
	 
	 index(test_id),
	 index(last_updated),
	 index(submission_time),
	 index(user_id),
	 index(platform_id),
	 index(opsys_id),
	 index(branch_id),
	 index(user_agent),
	 index(result_id),
	 index(build_type_id),
	 index(machine_name),
	 index(exit_status_id),
	 index(duration_ms),
	 index(talkback_id),
	 index(validity_id),
	 index(vetting_status_id),
	 index(locale_abbrev)';
	 
$table{test_status_lookup} = 
	'test_status_id tinyint not null primary key auto_increment,
	 name varchar(64) not null,
	 
	 index(name)';
	 
$table{tests} = 
	'test_id int not null primary key auto_increment,
	 subgroup_id smallint not null,
	 summary varchar(255) not null,
	 details text,
	 status_id tinyint not null,
	 community_enabled tinyint(1),
	 format_id tinyint not null default \'1\',
	 regression_bug_id int,
	 steps longtext,
	 expected_results longtext,
	 sort_order smallint not null default \'1\',
	 author_id int not null,
	 creation_date datetime not null,
	 last_updated datetime not null,
	 version smallint not null default \'1\',
	 testrunner_case_id int,
	 testrunner_case_version int,
	 
	 index(subgroup_id),
	 index(summary),
	 index(status_id),
	 index(community_enabled),
	 index(regression_bug_id),
	 index(steps(255)),
	 index(expected_results(255)),
	 index(author_id),
	 index(creation_date),
	 index(last_updated),
	 index(testrunner_case_id)';
	 
$table{users} = 
	'user_id int not null primary key auto_increment,
	 bugzilla_uid int,
	 email varchar(255),
	 password varchar(255),
	 realname varchar(255),
	 disabled mediumtext,
	 is_admin tinyint(1),
	 
	 index(bugzilla_uid),
	 index(email),
	 index(is_admin)';
	 
$table{sessions} =
	'session_id int not null primary key auto_increment,
	 user_id int not null,
	 sessioncookie varchar(255) not null,
	 expires datetime not null,
	 
	 index(user_id),
	 index(sessioncookie),
	 index(expires)';
	 
$table{validity_lookup} = 
	'validity_id tinyint not null primary key auto_increment,
	 name varchar(64) not null,
	
	 index(name)';
	
$table{vetting_status_lookup} = 
	'vetting_status_id tinyint not null primary key auto_increment,
	name varchar(64) not null,
	
	index(name)';