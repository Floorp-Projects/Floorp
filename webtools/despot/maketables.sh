#!/bin/sh
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Despot Account Administration System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>

mysql > /dev/null 2>/dev/null << OK_ALL_DONE

use mozusers;

drop table users;
drop table changes;
drop table syncneeded;

OK_ALL_DONE



mysql << OK_ALL_DONE




use mozusers;


create table repositories (
    id smallint not null auto_increment primary key,
    name varchar(255) not null,
    cvsroot varchar(255) not null,
    ownersrepository smallint,
    ownerspath varchar(255),
    domailing tinyint not null,    
    
    unique(name)
);

create table partitions (
    id mediumint not null auto_increment primary key,
    name varchar(255) not null,
    description mediumtext,
    repositoryid smallint not null,
    state enum("Open", "Restricted", "Closed"),
    branchid mediumint,
    newsgroups mediumtext,
    doclinks mediumtext,

    index(name),
    index(repositoryid)
    
);

create table files (
    partitionid mediumint not null,
    pattern varchar(255) not null,

    index(partitionid),
    index(pattern)
);

create table branches (
    id mediumint not null auto_increment primary key,
    name varchar(255) not null,

    unique(name)
);


create table members (
    userid mediumint not null,
    partitionid mediumint not null,
    class enum("Member", "Peer", "Owner") not null,

    unique(userid,partitionid),
    index(userid),
    index(partitionid),
    index(class)
);
    


create table users (
    id mediumint not null auto_increment primary key,
    email varchar(128) not null,
    realname varchar(255) not null,
    pserverhosts varchar(255),
    passwd varchar(64),
    gila_group enum("None", "webmonkey", "cvsadm"),
    cvs_group enum("None", "webmonkey", "cvsadm"),
    despot enum("No", "Yes"),
    neednewpassword enum("No", "Yes"),
    disabled enum("No", "Yes"),
    voucher mediumint not null,
    signedform enum("No", "Yes"),
    bugzillaid mediumint not null,

    unique(email)
);

show columns from users;
show index from users;

create table changes (
    email varchar(128) not null,
    field varchar(20) not null,
    oldvalue varchar(255),
    newvalue varchar(255),
    who varchar(32) not null,
    changed_when timestamp not null,
    index(changed_when)
);


show columns from changes;
show index from changes;


create table syncneeded (
    needed tinyint not null
);

insert into syncneeded (needed) values (1);

insert into branches (id,name) values (1, "HEAD");


OK_ALL_DONE
