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
    return $self->database($app)->execute("SELECT userData.userID
                                           FROM userData, userDataTypes
                                           WHERE userData.fieldID = userDataTypes.fieldID
                                           AND CONCAT(userDataTypes.data, userData.data) = ?", $username)->row;
    # return userID
}

sub getUserByID {
    my $self = shift;
    my($app, $id) = @_;
    $self->notImplemented();
    my @userData = $self->database($app)->execute("SELECT userID, mode, password, adminMessage, newFieldID, newFieldValue, newFieldKey
                                                   FROM user WHERE userID = ?", $id)->row;
    if (@userData) {
        # fields
        my $fieldData = $self->database($app)->execute("SELECT userData.fieldID, userData.data
                                                        FROM userData
                                                        WHERE userData.userID = ?", $id)->rows;
        my %fieldData = map { $$_[0] => $$_[1] } @$fieldData;
        # groups
        my $groupData = $self->database($app)->execute("SELECT groups.groupID, groups.name
                                                        FROM groups, userGroupsMapping
                                                        WHERE userGroupsMapping.userID = ?
                                                        AND userGroupsMapping.groupID = groups.groupID", $id)->rows;
        my %groupData = map { $$_[0] => $$_[1] } @$groupData;
        # rights
        my $rightsData = $self->database($app)->execute("SELECT rights.name
                                                         FROM rights, userGroupsMapping, groupRightsMapping
                                                         WHERE userGroupsMapping.userID = ?
                                                         AND userGroupsMapping.groupID = groupRightsMapping.groupID
                                                         AND groupRightsMapping.rightID = rights.rightID", $id)->rows;
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

# XXX XXX

sub setUser {
    my $self = shift;
    my($app, $userID, $mode, $password, $adminMessage, $newFieldID, $newFieldValue, $newFieldKey) = @_;
    # if userID is undefined, then add a new entry and return the
    # userID (so that it can be used in setUserField and
    # setUserGroups, later).
    $self->notImplemented();
}

sub setUserField
    my $self = shift;
    my($app, $userID, $fieldID, $data) = @_;
    $self->notImplemented();
}

sub removeUserField
    my $self = shift;
    my($app, $userID, $fieldID) = @_;
    $self->notImplemented();
}

sub setUserGroups
    my $self = shift;
    my($app, $userID, @groupIDs) = @_;
    $self->notImplemented();
}

# returns the userDataTypes table, basically...
sub getFields {
    my $self = shift;
    my($app) = @_;
    $self->notImplemented();
    # return [type, fieldID, category, name, typeData]*
}

sub getFieldByID {
    my $self = shift;
    my($app, $fieldID) = @_;
    return $self->database($app)->execute("SELECT type, fieldID, categorym name, data FROM userDataTypes WHERE fieldID = ?", $fieldID)->row;
}

sub getFieldByName {
    my $self = shift;
    my($app, $category, $name) = @_;
    $self->notImplemented();
    # return [type, fieldID, category, name, typeData]
}

sub setField {
    my $self = shift;
    my($app, $fieldID, $category, $name, $type, $data) = @_;
    # if fieldID is undefined, then add a new entry and return the
    # fieldID. Typically data will be undefined then too.
    $self->notImplemented();
}

sub removeField {
    my $self = shift;
    my($app, $fieldID) = @_;
    # This should handle the case where the field to be removed is
    # still referenced by some users
    $self->notImplemented();
}

sub getGroups {
    my $self = shift;
    my($app) = @_;
    $self->notImplemented();
    # return [groupID, name, [rightName]*]*
}

sub getGroupName {
    my $self = shift;
    my($app, $groupID) = @_;
    $self->notImplemented();
    # return name
}

sub setGroup {
    my $self = shift;
    my($app, $groupID, $groupName, @rightNames) = @_;
    # If groupID is undefined, then add a new entry and return the new
    # groupID. If groupName is undefined, then leave it as is. If both
    # groupID and groupName are undefined, there is an error.
    $self->assert(defined($groupID) or defined($groupName), 1, 
                  'Invalid arguments to DataSource::User::setGroup: \'groupID\' and \'groupName\' both undefined');
    $self->notImplemented();
}

sub getRights {
    my $self = shift;
    my($app, @groups) = @_;
    $self->notImplemented();
    # return [rightName]*
}

sub addRight {
    my $self = shift;
    my($app, $name) = @_;
    # only adds $name if it isn't there already
    $self->notImplemented();
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
                               mode integer unsigned DEFAULT 0,
                               adminMessage varchar(255),
                               newFieldID integer unsigned,
                               newFieldValue varchar(255),
                               newFieldKey varchar(255),
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
        # | data              | e.g. "ian@hixie.ch" or "1979-12-27" or an index into another table
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
                                  data text,
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
        # | data              | e.g. "SMS", "optional", null
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
