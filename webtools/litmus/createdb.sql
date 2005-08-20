create database litmus;
use litmus;

create table tests (
    test_id int(11) not null primary key auto_increment,
    subgroup_id smallint(6) not null, 
    summary varchar(255) not null, 
    details text,
    status_id tinyint(4) not null, 
    community_enabled boolean,
    format_id tinyint(4) not null, 
    regression_bug_id int(11),
    t1 longtext,
    t2 longtext,
    t3 longtext,
    
    INDEX(subgroup_id),
    INDEX(summary),
    INDEX(status_id),
    INDEX(community_enabled),
    INDEX(format_id),
    INDEX(regression_bug_id),
    INDEX(t1(255)),
    INDEX(t2(255)),
    INDEX(t3(255))
);

create table test_results (
    testresult_id int(11) not null primary key auto_increment,
    test_id int(11) not null,
    last_updated datetime not null,
    submission_time datetime not null,
    user_id int(11), 
    platform_id smallint(6), 
    opsys_id smallint(6), 
    branch_id smallint(6), 
    buildid varchar(45),
    user_agent varchar(255), 
    result_id smallint(6), 
    log_id int(11),
    
    INDEX (test_id),
    INDEX (last_updated),
    INDEX (submission_time),
    INDEX (user_id),
    INDEX (platform_id),
    INDEX (opsys_id),
    INDEX (branch_id),
    INDEX (user_agent),
    INDEX (result_id),
    INDEX (log_id)
);

create table products (
    product_id tinyint not null primary key auto_increment, 
    name varchar(64) not null,
    iconpath varchar(255), 
    
    INDEX(name),
    INDEX(iconpath)
);

create table test_groups (
    testgroup_id smallint(6) not null primary key auto_increment, 
    product_id tinyint(4) not null, 
    name varchar(64) not null,
    expiration_days smallint(6) not null,
    
    INDEX(product_id),
    INDEX(name),
    INDEX(expiration_days)
);

create table subgroups (
    subgroup_id smallint(6) not null primary key auto_increment, 
    testgroup_id smallint(6) not null, 
    name varchar(64) not null,
    
    INDEX(testgroup_id),
    INDEX(name)
);

create table test_status_lookup (
    test_status_id tinyint(4) not null primary key auto_increment, 
    name varchar(64) not null,
    
    INDEX(name)
);

create table platforms (
    platform_id smallint(6) not null primary key auto_increment,
    product_id tinyint(4) not null,
    name varchar(64) not null,
    detect_regexp varchar(255),
    iconpath varchar(255),
    
    INDEX(product_id),
    INDEX(name),
    INDEX(detect_regexp),
    INDEX(iconpath)
);

create table opsyses (
    opsys_id smallint(6) not null primary key auto_increment,
    platform_id smallint(6) not null, 
    name varchar(64) not null,
    detect_regexp varchar(255),
    
    INDEX(platform_id),
    INDEX(name),
    INDEX(detect_regexp)
);

create table branches (
    branch_id smallint(6) not null primary key auto_increment, 
    product_id tinyint(4) not null, 
    name varchar(64) not null,
    detect_regexp varchar(255),
    
    INDEX (product_id),
    INDEX (name),
    INDEX (detect_regexp)
);

create table test_result_status_lookup (
    result_status_id smallint(6) not null primary key auto_increment,
    name varchar(64) not null,
    style varchar(255) not null,
    
    INDEX(name),
    INDEX(style)
);

create table users (
    user_id int(11) not null primary key auto_increment,
    email varchar(255) not null,
    is_trusted boolean,
    
    INDEX(email),
    INDEX(is_trusted)
);

create table test_format_lookup (
    format_id tinyint(4) not null primary key auto_increment,
    name varchar(255) not null,
    
    INDEX(name)
);

create table test_result_bugs (
	test_result_id int(11) not null auto_increment,
	last_updated datetime not null,
	submission_time datetime not null,
	user_id int(11),
	bug_id int(11), 
	
	PRIMARY KEY (test_result_id,bug_id),
	
	INDEX(test_result_id),
	INDEX(last_updated),
	INDEX(submission_time),
	INDEX(user_id)
);

create table test_result_comments (
	comment_id int(11) not null primary key auto_increment,
	test_result_id int(11) not null, 
	last_updated datetime not null,
	submission_time datetime not null,
	user_id int(11),
	comment text,
	
	INDEX(test_result_id),
	INDEX(last_updated),
	INDEX(submission_time),
	INDEX(user_id)
);

create table test_result_logs (
	log_id int(11) not null primary key auto_increment,
	test_result_id int(11) not null,
	last_updated datetime not null,
	submission_time datetime not null,
	log_path varchar(255),
	
	INDEX(test_result_id),
	INDEX(last_updated),
	INDEX(submission_time),
	INDEX(log_path)
);