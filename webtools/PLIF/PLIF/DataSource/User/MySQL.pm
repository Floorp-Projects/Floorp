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
@ISA = qw(PLIF::DataSource::User);
1;

sub databaseType {
    return qw(mysql);
}

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
    my @userData = $self->database($app)->execute('SELECT userID, mode, password, adminMessage, newFieldID, newFieldValue, newFieldKey
                                                   FROM user WHERE userID = ?', $id)->row;
    if (@userData) {
        # fields
        my $fieldData = $self->database($app)->execute('SELECT userData.fieldID, userData.data
                                                        FROM userData
                                                        WHERE userData.userID = ?', $id)->rows;
        my %fieldData = map { $$_[0] => $$_[1] } @$fieldData;
        # groups
        my $groupData = $self->database($app)->execute('SELECT groups.groupID, groups.name
                                                        FROM groups, userGroupsMapping
                                                        WHERE userGroupsMapping.userID = ?
                                                        AND userGroupsMapping.groupID = groups.groupID', $id)->rows;
        my %groupData = map { $$_[0] => $$_[1] } @$groupData;
        # rights
        my $rightsData = $self->database($app)->execute('SELECT rights.name
                                                         FROM rights, userGroupsMapping, groupRightsMapping
                                                         WHERE userGroupsMapping.userID = ?
                                                         AND userGroupsMapping.groupID = groupRightsMapping.groupID
                                                         AND groupRightsMapping.rightID = rights.rightID', $id)->rows;
        my @rightsData = map { $$_[0] } @$rightsData;
        # bake it all up and send it to the customer
        return (@userData, \%fieldData, \%groupData, \@rightsData);
    } else {
        return ();
    }
    # return userID, mode, password, adminMessage, newFieldID,
    # newFieldValue, newFieldKey, { fieldID => data }*, { groupID => name }*,
    # [rightNames]*; or () if unsuccessful
}

sub setUser {
    my $self = shift;
    my($app, $userID, $mode, $password, $adminMessage, $newFieldID, $newFieldValue, $newFieldKey) = @_;
    # if userID is undefined, then add a new entry and return the
    # userID (so that it can be used in setUserField and
    # setUserGroups, later).
    if (defined($userID)) {
        $self->database($app)->execute('UPDATE user SET mode=?, password=?, adminMessage=?, newFieldID=?, newFieldValue=?, newFieldKey=?
                                        WHERE userID = ?', $mode, $password, $adminMessage, $newFieldID, $newFieldValue, $newFieldKey, $userID);
    } else {
        return $self->database($app)->execute('INSERT INTO user SET mode=?, password=?, adminMessage=?, newFieldID=?, newFieldValue=?, newFieldKey=?',
                                              $mode, $password, $adminMessage, $newFieldID, $newFieldValue, $newFieldKey)->MySQLID;
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

sub setUserGroups {
    my $self = shift;
    my($app, $userID, @groupIDs) = @_;
    $self->database($app)->execute('DELETE FROM userGroupsMapping WHERE userID = ?', $userID);
    my $handle = $self->database($app)->prepare('INSERT INTO userGroupsMapping SET userID=?, groupID=?');
    foreach my $groupID (@groupIDs) {
        $handle->reexecute($userID, $groupID);
    }
}

sub addUserGroup {
    my $self = shift;
    my($app, $userID, $groupID) = @_;
    $self->database($app)->execute('REPLACE INTO userGroupsMapping SET userID=?, groupID=?', $userID, $groupID);
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

sub setField {
    my $self = shift;
    my($app, $fieldID, $category, $name, $type, $data, $mode) = @_;
    # if fieldID is undefined, then add a new entry and return the
    # fieldID. $data will often be undefined or empty
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
    my $groups = $self->database($app)->execute('SELECT groups.groupID, groups.name FROM groups')->rows;
    foreach my $group (@$groups) {
        # rights
        my $rights = $self->database($app)->execute('SELECT rights.name
                                                     FROM rights, groupRightsMapping
                                                     AND userGroupsMapping.groupID = ?
                                                     AND groupRightsMapping.rightID = rights.rightID', $group->[0])->rows;
        foreach my $right (@$rights) {
            $right = $right->[0];
        }
        push(@$group, $rights);
    }
    return $groups;
    # return [groupID, name, [rightName]*]*
}

sub getGroupName {
    my $self = shift;
    my($app, $groupID) = @_;
    my $row = $self->database($app)->execute('SELECT name FROM groups WHERE groupID = ?', $groupID)->row;
    if (defined($row)) {
        return $row->[0];
    } else {
        return undef;
    }
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

sub getRights {
    my $self = shift;
    my($app, @groups) = @_;
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
    eval {
        $self->database($app)->execute('INSERT INTO rights SET name=?', $name);
    };
    if ($@) {
        # check for a mySQL specific error code for 'duplicate' and
        # reraise the error if it wasn't the problem
        if ($self->database($app)->lastError ne '1062') {
            $self->error(1, $@); # reraise
        }
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
    $self->dump(9, 'about to configure user data source...');
    if (not $helper->tableExists($app, $self->database($app), 'user')) {
        $self->debug('going to create \'user\' table');
        $app->output->setupProgress('user data source (creating user database)');
        $self->database($app)->execute('
            CREATE TABLE user (
                               userID integer unsigned auto_increment NOT NULL PRIMARY KEY,
                               password varchar(255) NOT NULL,
                               mode integer unsigned NOT NULL DEFAULT 0,
                               adminMessage varchar(255),
                               newFieldID integer unsigned,
                               newFieldValue varchar(255),
                               newFieldKey varchar(255)
                               )
        ');
        # +-------------------+
        # | user              |
        # +-------------------+
        # | userID         K1 | auto_increment
        # | password          |
        # | mode              | active, disabled, logging out, etc
        # | adminMessage      | string displayed when user (tries to) log in
        # | newFieldID        | \
        # | newFieldValue     |  > used when user tries to change his e-mail
        # | newFieldKey       | /  address, for example
        # +-------------------+
    } else {
        # check its schema is up to date
    }
    if (not $helper->tableExists($app, $self->database($app), 'userData')) {
        $self->debug('going to create \'userData\' table');
        $app->output->setupProgress('user data source (creating userData database)');
        $self->database($app)->execute('
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
    if (not $helper->tableExists($app, $self->database($app), 'userDataTypes')) {
        $self->debug('going to create \'userDataTypes\' table');
        $app->output->setupProgress('user data source (creating userDataTypes database)');
        $self->database($app)->execute('
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
        # | category       K2 | e.g. contact, personal, setting [1]
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
    if (not $helper->tableExists($app, $self->database($app), 'userGroupsMapping')) {
        $self->debug('going to create \'userGroupsMapping\' table');
        $app->output->setupProgress('user data source (creating userGroupsMapping database)');
        $self->database($app)->execute('
            CREATE TABLE userGroupsMapping (
                                  userID integer unsigned NOT NULL,
                                  groupID integer unsigned NOT NULL,
                                  PRIMARY KEY (userID, groupID)
                                  )
        ');
        # +-------------------+
        # | userGroupsMapping |
        # +-------------------+
        # | userID         K1 |
        # | groupID        K1 |
        # +-------------------+
    } else {
        # check its schema is up to date
    }
    if (not $helper->tableExists($app, $self->database($app), 'groups')) {
        $self->debug('going to create \'groups\' table');
        $app->output->setupProgress('user data source (creating groups database)');
        $self->database($app)->execute('
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
    } else {
        # check its schema is up to date
    }
    if (not $helper->tableExists($app, $self->database($app), 'groupRightsMapping')) {
        $self->debug('going to create \'groupRightsMapping\' table');
        $app->output->setupProgress('user data source (creating groupRightsMapping database)');
        $self->database($app)->execute('
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
    if (not $helper->tableExists($app, $self->database($app), 'rights')) {
        $self->debug('going to create \'rights\' table');
        $app->output->setupProgress('user data source (creating rights database)');
        $self->database($app)->execute('
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
    $self->dump(9, 'done configuring user data source');
    return;
}
