#!/bin/sh
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

mysql > /dev/null 2>/dev/null << OK_ALL_DONE

use bonsai;

drop table descs;
drop table checkins;
drop table people;
drop table repositories;
drop table dirs;
drop table files;
drop table branches;
drop table tags;
OK_ALL_DONE



mysql << OK_ALL_DONE
use bonsai;
create table descs (
    id mediumint not null auto_increment primary key,
    description text
);

show columns from descs;
show index from descs;
	
create table people (
    id mediumint not null auto_increment primary key,
    who varchar(16) not null,

    unique(who)
);

show columns from people;
show index from people;
	
	
create table repositories (
    id mediumint not null auto_increment primary key,
    repository varchar(64) not null,

    unique(repository)
);

show columns from repositories;
show index from repositories;



create table dirs (
    id mediumint not null auto_increment primary key,
    dir varchar(128) not null,

    unique(dir)
);

show columns from dirs;
show index from dirs;


create table files (
    id mediumint not null auto_increment primary key,
    file varchar(128) not null,

    unique(file)
);

show columns from files;
show index from files;

	

create table branches (
    id mediumint not null auto_increment primary key,
    branch varchar(64) not null,

    unique(branch)
);

show columns from branches;
show index from branches;

	

create table checkins (
	type enum('Change', 'Add', 'Remove'),
	when datetime not null,
	whoid mediumint not null,
	repositoryid mediumint not null,
	dirid mediumint not null,
	fileid mediumint not null,
	revision varchar(32) not null,
	stickytag varchar(255) not null,
	branchid mediumint not null,
	addedlines int not null,
	removedlines int not null,
	descid mediumint,

	unique (repositoryid,dirid,fileid,revision),
	index(when),
	index(whoid),
	index(repositoryid),
	index(dirid),
	index(fileid),
	index(branchid)
);

show columns from checkins;
show index from checkins;


create table tags (
	repositoryid mediumint not null,
	branchid mediumint not null,
	dirid mediumint not null,
	fileid mediumint not null,
	revision varchar(32) not null,

	unique(repositoryid,dirid,fileid,branchid,revision),
	index(repositoryid),
	index(dirid),
	index(fileid),
	index(branchid)
);
	


create table checkinlocks (
	repositoryid mediumint not null,
	branchid mediumint not null,
	dirid mediumint not null,
	fileid mediumint not null,
	revision


OK_ALL_DONE
