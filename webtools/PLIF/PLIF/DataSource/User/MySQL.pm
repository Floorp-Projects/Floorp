# -*- Mode: perl; tab-width: 4; indent-tabs-mode: nil; -*-
#
# This file is MPL/GPL dual-licensed under the following terms:
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and
# limitations under the License.
#
# The Original Code is PLIF 1.0.
# The Initial Developer of the Original Code is Ian Hickson.
#
# Alternatively, the contents of this file may be used under the terms
# of the GNU General Public License Version 2 or later (the "GPL"), in
# which case the provisions of the GPL are applicable instead of those
# above. If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the MPL, indicate your decision by
# deleting the provisions above and replace them with the notice and
# other provisions required by the GPL. If you do not delete the
# provisions above, a recipient may use your version of this file
# under either the MPL or the GPL.

package PLIF::DataSource::User::MySQL;
use strict;
use vars qw(@ISA);
use PLIF::DataSource::User;
use PLIF::Exception;
@ISA = qw(PLIF::DataSource::User);
1;

sub databaseType {
    return qw(mysql);
}

__DATA__

sub getUserIDByUsername {
    my $self = shift;
    my($app, $username) = @_;
    # the username for a user field is created by appending the 'data'
    # of the user field to the type data of the field description. For
    # example, for the field 'contact.icq', the type data field might
    # contain the string 'ICQ:' and the user field might be '55378571'
    # making the username 'ICQ:55378571'.
    return $self->database($app)->execute('SELECT userData.userID
                                           FROM userData, userDataTypes
                                           WHERE userData.fieldID = userDataTypes.fieldID
                                           AND userDataTypes.category = \'contact\'
                                           AND CONCAT(userDataTypes.data, userData.data) = ?', $username)->row;
    # return userID or undef
}

sub getUserIDByContactDetails {
    my $self = shift;
    my($app, $contactName, $address) = @_;
    return $self->database($app)->execute('SELECT userData.userID
                                           FROM userData, userDataTypes
                                           WHERE userData.fieldID = userDataTypes.fieldID
                                           AND userDataTypes.category = \'contact\'
                                           AND userDataTypes.name = ?
                                           AND userData.data = ?', $contactName, $address)->row;
    # return userID or undef
}

sub getUserByID {
    my $self = shift;
    my($app, $id) = @_;
    my @userData = $self->database($app)->execute('SELECT userID, mode, password, adminMessage
                                                   FROM user WHERE userID = ?', $id)->row;
    if (@userData) {
        # fields
        my $fieldData = $self->database($app)->execute('SELECT userData.fieldID, userData.data
                                                        FROM userData
                                                        WHERE userData.userID = ?', $id)->rows;
        my %fieldData = map { $$_[0] => $$_[1] } @$fieldData;
        # groups
        my $groupData = $self->database($app)->execute('SELECT groups.groupID, groups.name, userGroupsMapping.level
                                                        FROM groups, userGroupsMapping
                                                        WHERE userGroupsMapping.userID = ?
                                                        AND userGroupsMapping.groupID = groups.groupID', $id)->rows;
        # rights
        my $rightsData = $self->database($app)->execute('SELECT rights.name
                                                         FROM rights, userGroupsMapping, groupRightsMapping
                                                         WHERE userGroupsMapping.userID = ?
                                                         AND userGroupsMapping.groupID = groupRightsMapping.groupID
                                                         AND groupRightsMapping.rightID = rights.rightID', $id)->rows;
        my @rightsData = map { $$_[0] } @$rightsData;
        # bake it all up and send it to the customer
        return (@userData, \%fieldData, $groupData, \@rightsData);
    } else {
        return ();
    }
    # return userID, mode, password, adminMessage,
    # [ fieldID, data ]*, [ groupID, name, level ]*,
    # [rightNames]*; or () if unsuccessful
}

sub setUser {
    my $self = shift;
    my($app, $userID, $mode, $password, $adminMessage) = @_;
    # if userID is undefined, then add a new entry and return the
    # userID (so that it can be used in setUserField and
    # setUserGroups, later).
    if (defined($userID)) {
        $self->database($app)->execute('UPDATE user SET mode=?, password=?, adminMessage=?
                                        WHERE userID = ?', $mode, $password, $adminMessage, $userID);
        return $userID;
    } else {
        return $self->database($app)->execute('INSERT INTO user SET mode=?, password=?, adminMessage=?',
                                              $mode, $password, $adminMessage)->MySQLID;
    }
}

sub setUserField {
    my $self = shift;
    my($app, $userID, $fieldID, $data) = @_;
    # $data may be undefined and frequently will be if the row doesn't
    # exist yet
    $self->database($app)->execute('REPLACE INTO userData SET userID=?, fieldID=?, data=?', $userID, $fieldID, $data);
}

sub removeUserField {
    my $self = shift;
    my($app, $userID, $fieldID) = @_;
    $self->database($app)->execute('DELETE FROM userData WHERE userID = ? AND fieldID = ?', $userID, $fieldID);
}

sub setUserFieldChange {
    my $self = shift;
    my($app, $userID, $fieldID, $newData, $password, $type) = @_;
    return $self->database($app)->execute('INSERT INTO userDataChanges SET userID=?, fieldID=?, newData=?, password=?, type=?', $userID, $fieldID, $newData, $password, $type)->MySQLID;
}

sub getUserFieldChangeFromChangeID {
    my $self = shift;
    my($app, $changeID) = @_;
    return $self->database($app)->execute('SELECT userID, fieldID, newData, password, createTime, type FROM userDataChanges WHERE changeID = ?', $changeID)->row;
}

sub getUserFieldChangesFromUserIDAndFieldID {
    my $self = shift;
    my($app, $userID, $fieldID) = @_;
    return $self->database($app)->execute('SELECT changeID, newData, password, createTime, type FROM userDataChanges WHERE userID = ? AND fieldID = ?', $userID, $fieldID)->rows;
}

sub removeUserFieldChangeByChangeID {
    my $self = shift;
    my($app, $changeID) = @_;
    $self->database($app)->execute('DELETE FROM userDataChanges WHERE changeID = ?', $changeID);
}

sub removeUserFieldChangesByUserIDAndFieldID {
    my $self = shift;
    my($app, $userID, $fieldID) = @_;
    $self->database($app)->execute('DELETE FROM userDataChanges WHERE userID = ? AND fieldID = ?', $userID, $fieldID);
}

sub setUserGroups {
    my $self = shift;
    my($app, $userID, $groupIDs) = @_; # $groupIDs is a hash of groupID => level
    $self->database($app)->execute('DELETE FROM userGroupsMapping WHERE userID = ?', $userID);
    my $handle = $self->database($app)->prepare('INSERT INTO userGroupsMapping SET userID=?, groupID=?, level=?');
    foreach my $groupID (keys(%$groupIDs)) {
        $self->assert($groupIDs->{$groupID} > 0, 1, 'Invalid group membership level passed to setUserGroups');
        $handle->reexecute($userID, $groupID, $groupIDs->{$groupID});
    }
}

sub addUserGroup {
    my $self = shift;
    my($app, $userID, $groupID, $level) = @_;
    $self->assert($level > 0, 1, 'Invalid group membership level passed to addUserGroup');
    $self->database($app)->execute('REPLACE INTO userGroupsMapping SET userID=?, groupID=?, level=?', $userID, $groupID, $level);
}

sub removeUserGroup {
    my $self = shift;
    my($app, $userID, $groupID) = @_;
    $self->database($app)->execute('DELETE FROM userGroupsMapping WHERE userID = ? AND groupID = ?', $userID, $groupID);
}

# returns the userDataTypes table, basically...
sub getFields {
    my $self = shift;
    my($app) = @_;
    return $self->database($app)->execute('SELECT type, fieldID, category, name, data, mode FROM userDataTypes')->rows;
    # return [type, fieldID, category, name, typeData, mode]*, or something close to that... XXX
}

sub getFieldByID {
    my $self = shift;
    my($app, $fieldID) = @_;
    return $self->database($app)->execute('SELECT type, fieldID, category, name, data, mode FROM userDataTypes WHERE fieldID = ?', $fieldID)->row;
}

sub getFieldByName {
    my $self = shift;
    my($app, $category, $name) = @_;
    return $self->database($app)->execute('SELECT type, fieldID, category, name, data, mode FROM userDataTypes WHERE category = ? AND name = ?', $category, $name)->row;
    # return [type, fieldID, category, name, typeData]
}

sub getFieldNamesByCategory {
    my $self = shift;
    my($app, $category) = @_;
    my $rows = $self->database($app)->execute('SELECT name FROM userDataTypes WHERE category = ?', $category)->rows;
    foreach my $row (@$rows) {
        $row = $row->[0];
    }
    return @$rows;
    # return [name, name, name, name ...]
}

sub getFieldsByCategory {
    my $self = shift;
    my($app, $category) = @_;
    return $self->database($app)->execute('SELECT type, fieldID, category, name, data, mode FROM userDataTypes WHERE category = ?', $category)->rows;
    # return [type, fieldID, category, name, typeData]*
}

sub setField {
    my $self = shift;
    my($app, $fieldID, $category, $name, $type, $data, $mode) = @_;
    # if fieldID is undefined, then add a new entry and return the
    # fieldID. $data will often be undefined or empty
    $data = '' unless defined($data);
    $mode = 0 unless defined($mode);
    if (defined($fieldID)) {
        $self->database($app)->execute('UPDATE userDataTypes SET category=?, name=?, type=?, data=?, mode=? WHERE fieldID = ?',
                                       $category, $name, $type, $data, $mode, $fieldID);
        return $fieldID;
    } else {
        return $self->database($app)->execute('INSERT INTO userDataTypes SET category=?, name=?, type=?, data=?, mode=?',
                                              $category, $name, $type, $data, $mode)->MySQLID;
    }
}

sub removeField {
    my $self = shift;
    my($app, $fieldID) = @_;
    $self->database($app)->execute('DELETE FROM userData WHERE fieldID = ?', $fieldID);
    $self->database($app)->execute('DELETE FROM userDataTypes WHERE fieldID = ?', $fieldID);
}

sub getGroups {
    my $self = shift;
    my($app) = @_;
    # groups
    # XXX this should be optimised to one query
    my $groups = $self->database($app)->execute('SELECT groups.groupID, groups.name FROM groups')->rows;
    foreach my $group (@$groups) {
        # rights
        my $rights = $self->database($app)->execute('SELECT rights.name
                                                     FROM rights, groupRightsMapping
                                                     WHERE groupRightsMapping.groupID = ?
                                                     AND groupRightsMapping.rightID = rights.rightID', $group->[0])->rows;
        foreach my $right (@$rights) {
            $right = $right->[0];
        }
        push(@$group, $rights);
    }
    return $groups;
    # return [groupID, name, [rightName]*]*
}

sub getGroupMembers {
    my $self = shift;
    my($app, $groupID) = @_;
    return $self->database($app)->execute('SELECT userGroupsMapping.userID, userGroupsMapping.level
                                           FROM userGroupsMapping
                                           WHERE userGroupsMapping.groupID = ?', $groupID)->rows;
}

sub getGroupName {
    my $self = shift;
    my($app, $groupID) = @_;
    return scalar($self->database($app)->execute('SELECT name FROM groups WHERE groupID = ?', $groupID)->row);
    # return name or undef
}

sub setGroup {
    my $self = shift;
    my($app, $groupID, $groupName, @rightNames) = @_;
    # If groupID is undefined, then add a new entry and return the new
    # groupID. If groupName is undefined, then leave it as is. If both
    # groupID and groupName are undefined, there is an error.
    # This probably doesn't need to be too efficient...
    $self->assert(defined($groupID) or defined($groupName), 1,
                  'Invalid arguments to DataSource::User::setGroup: \'groupID\' and \'groupName\' both undefined');
    if (not defined($groupID)) {
        # add a new record
        $groupID = $self->database($app)->execute('INSERT INTO groups SET name=?', $groupName)->MySQLID;
    } elsif (defined($groupName)) {
        # replace the existing record
        $self->database($app)->execute('UPDATE groups SET name=? WHERE groupID = ?', $groupName, $groupID);
    }
    # now update the rights mapping table
    $self->database($app)->execute('DELETE FROM groupRightsMapping WHERE groupID = ?', $groupID);
    my $handle = $self->database($app)->prepare('INSERT INTO groupRightsMapping SET groupID=?, rightID=?');
    foreach my $right (@rightNames) {
        $handle->reexecute($groupID, $self->getRightID($app, $right));
    }
    return $groupID;
}

sub getRightsForGroups {
    my $self = shift;
    my($app, @groups) = @_;
    my $rights = $self->database($app)->execute('SELECT rights.name FROM rights, groupRightsMapping WHERE groupRightsMapping.rightID = rights.rightID AND groupRightsMapping.groupID IN(' .
                                                ('?, ' x (@groups-1)) . '?'.')', @groups)->rows;
    foreach my $right (@$rights) {
        $right = $right->[0];
    }
    return $rights;
    # return [rightName]*
}

sub getAllRights {
    my $self = shift;
    my($app) = @_;
    my $rights = $self->database($app)->execute('SELECT rights.name FROM rights')->rows;
    foreach my $right (@$rights) {
        $right = $right->[0];
    }
    return $rights;
    # return [rightName]*
}

sub addRight {
    my $self = shift;
    my($app, $name) = @_;
    $self->assert($name, 1, 'Tried to add a right without a right name');
    # only adds $name if it isn't there already, because name is a unique index
    try {
        $self->database($app)->execute('INSERT INTO rights SET name=?', $name);
    } catch PLIF::Exception::Database::Duplicate with {
        # ignore this error, reraise the others
    }
}


# internal routines

sub getRightID {
    my $self = shift;
    my($app, $name) = @_;
    my $row = $self->database($app)->execute('SELECT rightID FROM rights WHERE name = ?', $name)->row;
    if (defined($row)) {
        return $row->[0];
    } else {
        return undef;
    }
    # return rightID or undef
}

sub setupInstall {
    my $self = shift;
    my($app) = @_;
    my $helper = $self->helper($app);
    my $database = $self->database($app);
    if (not $helper->tableExists($app, $database, 'user')) {
        $app->output->setupProgress('dataSource.user.user');
        $database->execute('
            CREATE TABLE user (
                                  userID integer unsigned auto_increment NOT NULL PRIMARY KEY,
                                  password varchar(255) NOT NULL,
                                  mode integer unsigned NOT NULL DEFAULT 0,
                                  adminMessage varchar(255)
                                  )
        ');
        # +-------------------+
        # | user              |
        # +-------------------+
        # | userID         K1 | auto_increment
        # | password          |
        # | mode              | 0 = active, 1 = account disabled
        # | adminMessage      | string displayed when user (tries to) log in
        # +-------------------+
    } else {
        $app->output->setupProgress('dataSource.user.user.schemaChanges');
        # check its schema is up to date
        # delete user table's newField* fields.
        # note: can't move this data because new format uses more
        # fields, such as changeID, which old format knew nothing
        # about.
        if ($helper->columnExists($app, $database, 'user', 'newFieldID')) {
            $database->execute('ALTER TABLE user DROP COLUMN newFieldID');
        }
        if ($helper->columnExists($app, $database, 'user', 'newFieldValue')) {
            $database->execute('ALTER TABLE user DROP COLUMN newFieldValue');
        }
        if ($helper->columnExists($app, $database, 'user', 'newFieldKey')) {
            $database->execute('ALTER TABLE user DROP COLUMN newFieldKey');
        }
    }
    if (not $helper->tableExists($app, $database, 'userData')) {
        $app->output->setupProgress('dataSource.user.userData');
        $database->execute('
            CREATE TABLE userData (
                                  userID integer unsigned NOT NULL,
                                  fieldID integer unsigned NOT NULL,
                                  data text,
                                  PRIMARY KEY (userID, fieldID)
                                  )
        ');
        # +-------------------+
        # | userData          |
        # +-------------------+
        # | userID         K1 | points to entries in the table above
        # | fieldID        K1 | points to entries in the table below
        # | data              | e.g. 'ian@hixie.ch' or '1979-12-27' or an index into another table
        # +-------------------+
    } else {
        # check its schema is up to date
    }
    if (not $helper->tableExists($app, $database, 'userDataChanges')) {
        $app->output->setupProgress('dataSource.user.userDataChanges');
        $database->execute('
            CREATE TABLE userDataChanges (
                                  changeID integer unsigned auto_increment NOT NULL PRIMARY KEY,
                                  userID integer unsigned NOT NULL,
                                  fieldID integer unsigned NOT NULL,
                                  newData text,
                                  password varchar(255) NOT NULL,
                                  createTime TIMESTAMP DEFAULT NULL,
                                  type integer unsigned NOT NULL DEFAULT 0,
                                  KEY (userID, fieldID)
                                  )
        ');
        # +-------------------+
        # | userDataChanges   |
        # +-------------------+
        # | changeID       K1 | auto_increment
        # | userID         K2 | points to entries in the table above
        # | fieldID        K2 | points to entries in the table below
        # | newData           | e.g. 'ian@hixie.ch'
        # | password          | encrypted password
        # | createTime        | datetime
        # | type              | 0 = normal, 1 = remove all other user/field reqs
        # +-------------------+
    } else {
        # check its schema is up to date
    }
    if (not $helper->tableExists($app, $database, 'userDataTypes')) {
        $app->output->setupProgress('dataSource.user.userDataTypes');
        $database->execute('
            CREATE TABLE userDataTypes (
                                  fieldID integer unsigned auto_increment NOT NULL PRIMARY KEY,
                                  category varchar(64) NOT NULL,
                                  name varchar(64) NOT NULL,
                                  type varchar(64) NOT NULL,
                                  data text NOT NULL DEFAULT \'\',
                                  mode integer unsigned NOT NULL DEFAULT 0,
                                  UNIQUE KEY (category, name)
                                  )
        ');
        # +-------------------+
        # | userDataTypes     |
        # +-------------------+
        # | fieldID        K1 | auto_increment
        # | category       K2 | e.g. contact, personal, settings [1]
        # | name           K2 | e.g. sms, homepage, notifications [1]
        # | type              | e.g. number, string, notifications [2]
        # | data              | e.g. 'SMS', 'optional', null
        # | mode              | enabled, disabled, hidden, etc
        # +-------------------+
        # [1] used to find the fieldID for a particular category.name combination
        # [2] used to find the factory for the relevant user field object
    } else {
        # check its schema is up to date
    }
    if (not $helper->tableExists($app, $database, 'userGroupsMapping')) {
        $app->output->setupProgress('dataSource.user.userGroupsMapping');
        $database->execute('
            CREATE TABLE userGroupsMapping (
                                  userID integer unsigned NOT NULL,
                                  groupID integer unsigned NOT NULL,
                                  level integer unsigned NOT NULL DEFAULT 1,
                                  PRIMARY KEY (userID, groupID)
                                  )
        ');
        # +-------------------+
        # | userGroupsMapping |
        # +-------------------+
        # | userID         K1 |
        # | groupID        K1 |
        # | level             | 1: member, 2: admin, 3: owner [1]
        # +-------------------+
        # [1]: level 0 is reserved for 'not a member' which in the database is represented by absence
    } else {
        $app->output->setupProgress('dataSource.user.userGroupsMapping.schemaChanges');
        # check its schema is up to date
        if (not $helper->columnExists($app, $database, 'userGroupsMapping', 'level')) {
            $database->execute('ALTER TABLE userGroupsMapping ADD COLUMN level integer unsigned NOT NULL DEFAULT 1');
        }
    }
    if (not $helper->tableExists($app, $database, 'groups')) {
        $app->output->setupProgress('dataSource.user.groups');
        $database->execute('
            CREATE TABLE groups (
                                  groupID integer unsigned auto_increment NOT NULL PRIMARY KEY,
                                  name varchar(255) NOT NULL,
                                  UNIQUE KEY (name)
                                  )
        ');
        # +-------------------+
        # | groups            |
        # +-------------------+
        # | groupID        K1 | auto_increment
        # | name           K2 | user defined name (can be changed)
        # +-------------------+
        $self->database($app)->execute('INSERT INTO groups SET groupID=?, name=?', 1, 'Administrators');
    } else {
        # check its schema is up to date
        if (not $self->database($app)->execute('SELECT groupID FROM groups WHERE groupID = ?', 1)->row) {
            $self->database($app)->execute('INSERT INTO groups SET groupID=?, name=?', 1, 'Administrators');
        }
    }
    if (not $helper->tableExists($app, $database, 'groupRightsMapping')) {
        $app->output->setupProgress('dataSource.user.groupRightsMapping');
        $database->execute('
            CREATE TABLE groupRightsMapping (
                                  groupID integer unsigned NOT NULL,
                                  rightID integer unsigned NOT NULL,
                                  PRIMARY KEY (groupID, rightID)
                                  )
        ');
        # +-------------------+
        # | groupRightsMapping|
        # +-------------------+
        # | groupID        K1 |
        # | rightID        K1 |
        # +-------------------+
    } else {
        # check its schema is up to date
    }
    if (not $helper->tableExists($app, $database, 'rights')) {
        $app->output->setupProgress('dataSource.user.rights');
        $database->execute('
            CREATE TABLE rights (
                                  rightID integer unsigned auto_increment NOT NULL PRIMARY KEY,
                                  name varchar(255) NOT NULL,
                                  UNIQUE KEY (name)
                                  )
        ');
        # +-------------------+
        # | rights            |
        # +-------------------+
        # | rightID        K1 | implementation detail - not ever passed to other parts of the code
        # | name           K2 | the internal name for the right, as used by the code
        # +-------------------+
    } else {
        # check its schema is up to date
    }
    return;
}
