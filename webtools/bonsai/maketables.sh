#!/bin/sh
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

USER=nobody
PASSWORD=

if test x$PASSWORD = x ; then
  MYSQL="mysql -u $USER"
else
  MYSQL="mysql -u $USER -p$PASSWORD"
fi

echo
echo "Will use user=\"$USER\" and password=\"$PASSWORD\" for bonsai database."
echo "If you have a previous bonsai install, this script will drop all"
echo "bonsai tables. Press ctrl-c to bail out now or return to continue."

read dummy

echo Dropping old tables

$MYSQL << OK_ALL_DONE

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

echo creating new tables

$MYSQL << OK_ALL_DONE
use bonsai;
create table descs (
    id mediumint not null auto_increment primary key,
    description text,
    hash bigint not null,

    index(hash)
);

show columns from descs;
show index from descs;
	
create table people (
    id mediumint not null auto_increment primary key,
    who varchar(32) binary not null,

    unique(who)
);

show columns from people;
show index from people;
	
	
create table repositories (
    id mediumint not null auto_increment primary key,
    repository varchar(64) binary not null,

    unique(repository)
);

show columns from repositories;
show index from repositories;



create table dirs (
    id mediumint not null auto_increment primary key,
    dir varchar(255) binary not null,

    unique(dir)
);

show columns from dirs;
show index from dirs;


create table files (
    id mediumint not null auto_increment primary key,
    file varchar(255) binary not null,

    unique(file)
);

show columns from files;
show index from files;

	

create table branches (
    id mediumint not null auto_increment primary key,
    branch varchar(64) binary not null,

    unique(branch)
);

show columns from branches;
show index from branches;

	

create table checkins (
	type enum('Change', 'Add', 'Remove'),
	ci_when datetime not null,
	whoid mediumint not null,
	repositoryid mediumint not null,
	dirid mediumint not null,
	fileid mediumint not null,
	revision varchar(32) binary not null,
	stickytag varchar(255) binary not null,
	branchid mediumint not null,
	addedlines int not null,
	removedlines int not null,
	descid mediumint not null,

	unique (repositoryid,dirid,fileid,revision),
	index(ci_when),
	index(whoid),
	index(repositoryid),
	index(dirid),
	index(fileid),
	index(branchid),
	index(descid)
);

show columns from checkins;
show index from checkins;


create table tags (
	repositoryid mediumint not null,
	branchid mediumint not null,
	dirid mediumint not null,
	fileid mediumint not null,
	revision varchar(32) binary not null,

	unique(repositoryid,dirid,fileid,branchid,revision),
	index(repositoryid),
	index(dirid),
	index(fileid),
	index(branchid)
);
	


OK_ALL_DONE
