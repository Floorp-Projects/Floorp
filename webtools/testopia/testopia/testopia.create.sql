/*
Created		7/28/2005
Modified		5/12/2006
Project		Testrunner
Model		Based on Bugzilla 2.20
Company		Novell
Author		Jim Stutz
Version		12
Database		mySQL 4.0 
*/




Create table test_category_templates (
	category_template_id Smallint UNSIGNED NOT NULL AUTO_INCREMENT,
	name Varchar(255) NOT NULL,
	description Mediumtext,
	UNIQUE (category_template_id),
	UNIQUE (name),
	Index AI_category_template_id (category_template_id)) TYPE = MyISAM
COMMENT = 'Category templates store suggested values for categories.  Users, when creating categories, will have the option to use an existing template from this entity class or can save an existing category as a template.  Category templates are not associated with specific plans or products and are global to the system.';

Create table test_attachments (
	attachment_id Serial,
	plan_id Bigint UNSIGNED,
	case_id Bigint UNSIGNED,
	submitter_id Mediumint NOT NULL,
	description Mediumtext,
	filename Mediumtext,
	creation_ts Timestamp(14),
	mime_type Varchar(100) NOT NULL,
	Index AI_attachment_id (attachment_id)) TYPE = MyISAM
COMMENT = 'The test_attachment entity contains attachments that have been uploaded and can be associated with either test cases or test plans but not both. In other words, either a case_id or a plan_id will be stored but not both.';

Create table test_case_categories (
	category_id Smallint UNSIGNED NOT NULL AUTO_INCREMENT,
	product_id Smallint NOT NULL,
	name Varchar(240) NOT NULL,
	description Mediumtext,
	UNIQUE (category_id),
	Index AI_category_id (category_id)) TYPE = MyISAM
COMMENT = 'Categories are designed to group test cases in a test plan.   Categories can be anything the user wants to be. For example you could create an acceptance category to help identify acceptance test cases.   Categories are plan specific and can be copied with a plan when a plan is cloned.  Categories can also be based off category templates.';

Create table test_cases (
	case_id Serial,
	case_status_id Tinyint NOT NULL,
	category_id Smallint UNSIGNED NOT NULL,
	priority_id Smallint,
	author_id Mediumint NOT NULL,
	default_tester_id Mediumint,
	creation_date Datetime NOT NULL,
	isautomated Bool NOT NULL DEFAULT 0,
	sortkey Int,
	script Mediumtext,
	arguments Mediumtext,
	summary Varchar(255),
	requirement Varchar(255),
	alias Varchar(255),
	UNIQUE (case_id),
	UNIQUE (alias),
	Index AI_case_id (case_id)) TYPE = MyISAM
COMMENT = 'Test cases are the core of testrunner. A test case is essentially a checklist of things to check to determine whether the object under scrutiny passes or fails. A test case is associated with one or more test plans and is represented in a case run as a log entry (test_case_run). Cases consist of actions and effects which determine the passage or failure of the case in a run.';

Create table test_case_bugs (
	bug_id Mediumint NOT NULL,
	case_run_id Bigint UNSIGNED) TYPE = MyISAM
COMMENT = 'The test_case_bugs table is a junction and lists bugs found in test case runs.   Each test case run can have multiple bugs associated with it.';

Create table test_case_runs (
	case_run_id Serial,
	run_id Bigint UNSIGNED NOT NULL,
	case_id Bigint UNSIGNED NOT NULL,
	assignee Mediumint,
	testedby Mediumint,
	case_run_status_id Tinyint UNSIGNED NOT NULL,
	case_text_version Mediumint NOT NULL,
	build_id Bigint UNSIGNED NOT NULL,
	close_date Datetime,
	notes Text,
	iscurrent Bool DEFAULT 0,
	sortkey Int,
	UNIQUE (case_run_id),
	Index AI_case_run_id (case_run_id)) TYPE = MyISAM
COMMENT = 'When test cases are run in a test run, a row for each test case will be created in the test_case_runs table. Test_case_runs are logs of whether a particular test case passed or failed during a particular run. Previously known as test_case_logs.';

Create table test_case_texts (
	case_id Bigint UNSIGNED NOT NULL,
	case_text_version Mediumint NOT NULL,
	who Mediumint NOT NULL,
	creation_ts Timestamp(14) NOT NULL,
	action Mediumtext,
	effect Mediumtext) TYPE = MyISAM
COMMENT = 'As a test case document is changed the text is stored in a new row in this table. This increments the version of the testcase. It is important to note that changes to other fields in a test case besides the action and effect, are logged in the case activity table and do not increment the case version.';

Create table test_case_tags (
	tag_id Int UNSIGNED NOT NULL,
	case_id Bigint UNSIGNED) TYPE = MyISAM
COMMENT = 'A tag is a way of organizing information in a many to many relationship. Tags are associated with test cases via this junction table.';

Create table test_plan_testers (
	tester_id Mediumint NOT NULL,
	product_id Smallint,
	read_only Bool DEFAULT 1) TYPE = MyISAM
COMMENT = 'The testers who can edit or view the test_case, test_run, test_case_text for a given plan.';

Create table test_tags (
	tag_id Int UNSIGNED NOT NULL AUTO_INCREMENT,
	tag_name Varchar(255) NOT NULL,
	UNIQUE (tag_id),
	UNIQUE (tag_name),
	Index AI_tag_id (tag_id)) TYPE = MyISAM
COMMENT = 'Test tags are used to classify test cases in a many to many relationship. Tags are similar to keywords in bugzilla and are global to the testrunner system.';

Create table test_plans (
	plan_id Serial,
	product_id Smallint NOT NULL,
	author_id Mediumint NOT NULL,
	editor_id Mediumint NOT NULL,
	type_id Tinyint UNSIGNED NOT NULL,
	default_product_version Mediumtext NOT NULL,
	name Varchar(255) NOT NULL,
	creation_date Datetime NOT NULL,
	isactive Bool NOT NULL DEFAULT 1,
	UNIQUE (plan_id),
	Index AI_plan_id (plan_id)) TYPE = MyISAM
COMMENT = 'A test plan is the parent of all other entites within testrunner. The test plan details how a set of tests should be approached for a given product and contains cases and runs.';

Create table test_plan_texts (
	plan_id Bigint UNSIGNED NOT NULL,
	plan_text_version Int NOT NULL,
	who Mediumint NOT NULL,
	creation_ts Timestamp(14) NOT NULL,
	plan_text Longtext) TYPE = MyISAM
COMMENT = 'Each revision to the document of a test plan creates an entry in test_plan_texts. The document details the overarching plan for testing a product.';

Create table test_plan_types (
	type_id Tinyint UNSIGNED NOT NULL AUTO_INCREMENT,
	name Varchar(64) NOT NULL,
	UNIQUE (type_id),
	Index AI_type_id (type_id)) TYPE = MyISAM
COMMENT = 'The type of plan this is (e.g unit or integration)';

Create table test_runs (
	run_id Serial,
	plan_id Bigint UNSIGNED NOT NULL,
	environment_id Bigint UNSIGNED NOT NULL,
	product_version Mediumtext,
	build_id Bigint UNSIGNED NOT NULL,
	plan_text_version Int NOT NULL,
	manager_id Mediumint NOT NULL,
	start_date Datetime,
	stop_date Datetime,
	summary Tinytext NOT NULL,
	notes Mediumtext,
	UNIQUE (run_id),
	Index AI_run_id (run_id)) TYPE = MyISAM
COMMENT = 'Test runs are created from a plan and a set of testcases associated with that plan. The tes run entity tracks information about the run itself such as when it was started etc.';

Create table test_case_plans (
	plan_id Bigint UNSIGNED NOT NULL,
	case_id Bigint UNSIGNED NOT NULL) TYPE = MyISAM
COMMENT = 'The test_case_plans table is junction between test cases and test plans. Each test plan can have many test cases but likewise each case can be associated with multiple plans.';

Create table test_case_activity (
	case_id Bigint UNSIGNED NOT NULL,
	fieldid Smallint UNSIGNED NOT NULL,
	who Mediumint NOT NULL,
	changed Datetime NOT NULL,
	oldvalue Mediumtext,
	newvalue Mediumtext) TYPE = MyISAM
COMMENT = 'The test_case_activity table tracks changes made to test cases. Fields are mapped to the fielddefs table.';

Create table test_fielddefs (
	fieldid Smallint UNSIGNED NOT NULL AUTO_INCREMENT,
	name Varchar(100) NOT NULL,
	description Mediumtext,
	table_name Varchar(100) NOT NULL,
	UNIQUE (fieldid),
	Index AI_fieldid (fieldid)) TYPE = MyISAM
COMMENT = 'Test_fielddefs will contain a list of fields used in test cases, test plans, and test runs for these entities respective activity tables.  This is patterned after Bugzilla''s fielddefs table. All fields in any of these entites which you wish to track activity on should have an entry in this table, the ID of which will then be used to track activity in the activty tables.';

Create table test_plan_activity (
	plan_id Bigint UNSIGNED NOT NULL,
	fieldid Smallint UNSIGNED NOT NULL,
	who Mediumint NOT NULL,
	changed Datetime NOT NULL,
	oldvalue Mediumtext,
	newvalue Mediumtext) TYPE = MyISAM
COMMENT = 'Test plan activitiy tracks changes to non text fields of test plans.';

Create table test_case_components (
	case_id Bigint UNSIGNED NOT NULL,
	component_id Smallint NOT NULL) TYPE = MyISAM
COMMENT = 'The test_case_component table is a junction between test cases and components. Each test case can have zero or more components associated with it.';

Create table test_run_activity (
	run_id Bigint UNSIGNED NOT NULL,
	fieldid Smallint UNSIGNED NOT NULL,
	who Mediumint NOT NULL,
	changed Datetime NOT NULL,
	oldvalue Mediumtext,
	newvalue Mediumtext) TYPE = MyISAM
COMMENT = 'Tracks changes to a test run. Note that this is not the same as changes to a case run (or run log) entity of class test_case_run.';

Create table test_run_cc (
	run_id Bigint UNSIGNED NOT NULL,
	who Mediumint NOT NULL) TYPE = MyISAM
COMMENT = 'This is a list of people who should be notified about run status chages who may not otherwise be associated with the plan or it''s runs';

Create table test_email_settings (
	userid Mediumint NOT NULL,
	eventid Tinyint UNSIGNED NOT NULL,
	relationship_id Tinyint UNSIGNED NOT NULL) TYPE = MyISAM
COMMENT = 'A mapping of user email settings. Patterned after bugzilla email settings table.  This allows users to select which events will trigger email notification to themselves depending on the roles they assume in the system.';

Create table test_events (
	eventid Tinyint UNSIGNED NOT NULL,
	name Varchar(50),
	UNIQUE (eventid)) TYPE = MyISAM
COMMENT = 'events table stores a list of events which should trigger email notification.';

Create table test_relationships (
	relationship_id Tinyint UNSIGNED NOT NULL,
	name Varchar(50),
	UNIQUE (relationship_id)) TYPE = MyISAM
COMMENT = 'A list of relationships for use in tracking notifications. Who should be notified.';

Create table test_case_run_status (
	case_run_status_id Tinyint UNSIGNED NOT NULL AUTO_INCREMENT,
	name Varchar(20),
	sortkey Int,
	UNIQUE (sortkey),
	Index AI_case_run_status_id (case_run_status_id)) TYPE = MyISAM
COMMENT = 'The test_case_run_status entity describes values that can be used as statuses in test_case_runs. Previously and enum field.';

Create table test_case_status (
	case_status_id Tinyint NOT NULL AUTO_INCREMENT,
	name Varchar(255) NOT NULL,
	UNIQUE (case_status_id),
	Index AI_case_status_id (case_status_id)) TYPE = MyISAM
COMMENT = 'Test_case_status is a lookup of status on a test case. Replaces enum';

Create table test_case_dependencies (
	dependson Bigint UNSIGNED NOT NULL,
	blocked Bigint UNSIGNED) TYPE = MyISAM
COMMENT = 'Track case dependenncies.   Often test cases must be run in a specific order. This table will track which cases have dependencies on other cases.';

Create table test_case_group_map (
	case_id Bigint UNSIGNED NOT NULL,
	group_id Mediumint) TYPE = MyISAM;

Create table test_environments (
	environment_id Serial NOT NULL,
	op_sys_id Int,
	rep_platform_id Int,
	name Varchar(255),
	xml Mediumtext) TYPE = MyISAM;

Create table test_run_tags (
	tag_id Int UNSIGNED NOT NULL,
	run_id Bigint UNSIGNED) TYPE = MyISAM;

Create table test_plan_tags (
	tag_id Int UNSIGNED NOT NULL,
	plan_id Bigint UNSIGNED) TYPE = MyISAM;

Create table test_builds (
	build_id Serial NOT NULL,
	product_id Smallint NOT NULL,
	milestone Varchar(20),
	name Varchar(255),
	description Text) TYPE = MyISAM;

Create table test_attachment_data (
	attachment_id Bigint UNSIGNED NOT NULL,
	contents Longblob) TYPE = MyISAM;

Create table test_named_queries (
	userid Mediumint NOT NULL,
	name Varchar(64) NOT NULL,
	isvisible Bool NOT NULL,
	query Mediumtext NOT NULL) TYPE = MyISAM;



Create Index category_template_name_idx ON test_category_templates (name);
Create Index attachment_plan_idx ON test_attachments (plan_id);
Create Index attachment_case_idx ON test_attachments (case_id);
Create Index category_name_indx ON test_case_categories (name);
Create Index test_case_category_idx ON test_cases (category_id);
Create Index test_case_author_idx ON test_cases (author_id);
Create Index test_case_creation_date_idx ON test_cases (creation_date);
Create Index test_case_sortkey_idx ON test_cases (sortkey);
Create Index test_case_requirment_idx ON test_cases (requirement);
Create Index test_case_shortname_idx ON test_cases (alias);
Create UNIQUE Index case_run_id_idx ON test_case_bugs (case_run_id,bug_id);
Create Index case_run_bug_id_idx ON test_case_bugs (bug_id);
Create Index case_run_run_id_idx ON test_case_runs (run_id);
Create Index case_run_case_id_idx ON test_case_runs (case_id);
Create Index case_run_assignee_idx ON test_case_runs (assignee);
Create Index case_run_testedby_idx ON test_case_runs (testedby);
Create Index case_run_close_date_idx ON test_case_runs (close_date);
Create Index case_run_sortkey_idx ON test_case_runs (sortkey);
Create UNIQUE Index case_versions_idx ON test_case_texts (case_id,case_text_version);
Create Index case_versions_who_idx ON test_case_texts (who);
Create Index case_versions_creation_ts_idx ON test_case_texts (creation_ts);
Create Index case_tags_case_id_idx ON test_case_tags (case_id,tag_id);
Create Index case_tags_tag_id_idx ON test_case_tags (tag_id,case_id);
Create Index test_tag_name_idx ON test_tags (tag_name);
Create Index plan_product_plan_id_idx ON test_plans (product_id,plan_id);
Create Index plan_author_idx ON test_plans (author_id);
Create Index plan_type_idx ON test_plans (type_id);
Create Index plan_editor_idx ON test_plans (editor_id);
Create Index plan_isactive_idx ON test_plans (isactive);
Create Index plan_name_idx ON test_plans (name);
Create Index test_plan_text_version_idx ON test_plan_texts (plan_id,plan_text_version);
Create Index test_plan_text_who_idx ON test_plan_texts (who);
Create Index plan_type_name_idx ON test_plan_types (name);
Create Index test_run_plan_id_run_id__idx ON test_runs (plan_id,run_id);
Create Index test_run_manager_idx ON test_runs (manager_id);
Create Index test_run_start_date_idx ON test_runs (start_date);
Create Index test_run_stop_date_idx ON test_runs (stop_date);
Create Index case_plans_plan_id_idx ON test_case_plans (plan_id);
Create Index case_plans_case_id_idx ON test_case_plans (case_id);
Create Index case_activity_case_id_idx ON test_case_activity (case_id);
Create Index case_activity_who_idx ON test_case_activity (who);
Create Index case_activity_when_idx ON test_case_activity (changed);
Create Index case_activity_field_idx ON test_case_activity (fieldid);
Create Index fielddefs_name_idx ON test_fielddefs (name);
Create Index plan_activity_who_idx ON test_plan_activity (who);
Create Index case_components_case_id_idx ON test_case_components (case_id);
Create Index case_components_component_id_idx ON test_case_components (component_id);
Create Index run_activity_run_id_idx ON test_run_activity (run_id);
Create Index run_activity_who_idx ON test_run_activity (who);
Create Index run_activity_when_idx ON test_run_activity (changed);
Create Index run_cc_run_id_who_idx ON test_run_cc (run_id,who);
Create Index test_event_user_event_dx ON test_email_settings (userid,eventid);
Create Index test_event_user_relationship_idx ON test_email_settings (userid,relationship_id);
Create Index test_event_name_idx ON test_events (name);
Create Index case_run_status_name_idx ON test_case_run_status (name);
Create Index case_run_status_sortkey_idx ON test_case_run_status (sortkey);
Create Index test_case_status_name_idx ON test_case_status (name);
Create Index environment_op_sys_idx ON test_environments (op_sys_id);
Create Index environment_platform_idx ON test_environments (rep_platform_id);
Create UNIQUE Index environment_name_idx ON test_environments (name);
Create UNIQUE Index run_tags_idx ON test_run_tags (tag_id,run_id);
Create UNIQUE Index plan_tags_idx ON test_plan_tags (tag_id,plan_id);
Create Index test_namedquery_name_idx ON test_named_queries (name);

LOCK TABLES `test_case_run_status` WRITE;
INSERT INTO `test_case_run_status` VALUES (1,'IDLE',1),(2,'PASSED',2),(3,'FAILED',3),(4,'RUNNING',4),(5,'PAUSED',5),(6,'BLOCKED',6);
UNLOCK TABLES;

LOCK TABLES `test_case_status` WRITE;
INSERT INTO `test_case_status` VALUES (1,'PROPOSED'),(2,'CONFIRMED'),(3,'DISABLED');
UNLOCK TABLES;

LOCK TABLES `test_plan_types` WRITE;
INSERT INTO `test_plan_types` VALUES (1,'Unit'),(2,'Integration'),(3,'Function'),(4,'System'),(5,'Acceptance'),(6,'Installation'),(7,'Performance'),(8,'Product'),(9,'Interoperability');
UNLOCK TABLES;

LOCK TABLES `test_fielddefs` WRITE;
INSERT INTO `test_fielddefs` VALUES (1,'isactive','Archived','test_plans'),(2,'name','Plan Name','test_plans'),(3,'type_id','Plan Type','test_plans'),(4,'case_status_id','Case Status','test_cases'),(5,'category_id','Category','test_cases'),(6,'priority_id','Priority','test_cases'),(7,'summary','Run Summary','test_cases'),(8,'isautomated','Automated','test_cases'),(9,'alias','Alias','test_cases'),(10,'requirement','Requirement','test_cases'),(11,'script','Script','test_cases'),(12,'arguments','Argument','test_cases'),(13,'product_id','Product','test_plans'),(14,'default_product_version','Default Product Version','test_plans'),(15,'environment_id','Environment','test_runs'),(16,'product_version','Product Version','test_runs'),(17,'build_id','Default Build','test_runs'),(18,'plan_text_version','Plan Text Version','test_runs'),(19,'manager_id','Manager','test_runs'),(20,'default_tester_id','Default Tester','test_cases'),(21,'stop_date','Stop Date','test_runs'),(22,'summary','Run Summary','test_runs'),(23,'notes','Notes','test_runs');
UNLOCK TABLES;
