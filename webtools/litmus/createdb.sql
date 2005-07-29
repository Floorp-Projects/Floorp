create database litmus;
use litmus;

create table tests (
    testid int not null primary key auto_increment,
    subgroup smallint not null, 
    summary varchar(255), 
    status tinyint not null, 
    communityenabled boolean,
    format tinyint not null, 
    t1 longtext, 
    t2 longtext,
    t3 longtext,
    s1 varchar(255),
    s2 varchar(255),
    i1 mediumint, 
    i2 mediumint
);

create table testresults (
    testresultid int not null primary key auto_increment,
    testid int not null,
    timestamp datetime not null,
    user int, 
    platform smallint, 
    opsys smallint, 
    branch smallint, 
    buildid varchar(45),
    useragent varchar(255), 
    result smallint, 
    note text
);

create table products (
    productid tinyint not null primary key auto_increment, 
    name varchar(64) not null,
    iconpath varchar(64)
);

create table testgroups (
    testgroupid smallint not null primary key auto_increment, 
    product tinyint not null, 
    name varchar(64) not null,
    expirationdays smallint not null
);

create table subgroups (
    subgroupid smallint not null primary key auto_increment, 
    testgroup smallint not null, 
    name varchar(64) not null
);

create table statuses (
    statusid tinyint not null primary key auto_increment, 
    name varchar(64) not null
);

create table platforms (
    platformid smallint not null primary key auto_increment,
    product tinyint not null,
    name varchar(64) not null,
    detect_regexp varchar(255),
    iconpath varchar(64)
);

create table opsyses (
    opsysid smallint not null primary key auto_increment,
    platform smallint not null, 
    name varchar(64) not null,
    detect_regexp varchar(255)
);

create table branches (
    branchid smallint not null primary key auto_increment, 
    product tinyint not null, 
    name varchar(64) not null,
    detect_regexp varchar(255)
);

create table results (
    resultid smallint not null primary key auto_increment,
    name varchar(64) not null,
    style varchar(255) not null
);

create table users (
    userid int not null primary key auto_increment,
    email varchar(255) not null,
    istrusted boolean
);

create table formats (
    formatid tinyint not null primary key auto_increment,
    name varchar(255) not null
);