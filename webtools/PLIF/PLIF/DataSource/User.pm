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

package PLIF::DataSource::User;
use strict;
use vars qw(@ISA);
use PLIF::DataSource;
@ISA = qw(PLIF::DataSource);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'dataSource.user' or $service eq 'setup.install' or $class->SUPER::provides($service));
}

sub databaseName {
    return 'default';
}

sub getUserByUsername {
    my $self = shift;
    my($app, $username) = @_;
    # decent databases can do this in one go. Those that can't can do
    # it in a generic two-step process:
    my $userID = $self->getUserIDByUsername($app, $username);
    if (defined($userID)) {
        return $self->getUserByID($app, $userID);
    } else {
        return ();
    }
    # return the same as getUserByID()
}

sub getUserIDByUsername {
    my $self = shift;
    my($app, $username) = @_;
    $self->notImplemented();
    # return userID or undef
}

sub getUserByContactDetails {
    my $self = shift;
    my($app, $contactName, $address) = @_;
    # decent databases can do this in one go. Those that can't can do
    # it in a generic two-step process:
    my $userID = $self->getUserIDByContactDetails($app, $contactName, $address);
    if (defined($userID)) {
        return $self->getUserByID($app, $userID);
    } else {
        return ();
    }
    # return the same as getUserByID()
}

sub getUserIDByContactDetails {
    my $self = shift;
    my($app, $contactName, $address) = @_;
    $self->notImplemented();
    # return userID or undef
}

sub getUserByID {
    my $self = shift;
    my($app, $id) = @_;
    $self->notImplemented();
    # return userID, disabled, password, adminMessage, newFieldID, newFieldValue, 
    # newFieldKey, { fieldID => data }*, { groupID => name }*, [rightNames]*
    # or () if unsuccessful
}

sub setUser {
    my $self = shift;
    my($app, $userID, $disabled, $password, $adminMessage, $newFieldID, $newFieldValue, $newFieldKey) = @_;
    # if userID is undefined, then add a new entry and return the
    # userID (so that it can be used in setUserField and
    # setUserGroups, later).
    $self->notImplemented();
}

sub setUserField {
    my $self = shift;
    my($app, $userID, $fieldID, $data) = @_;
    $self->notImplemented();
}

sub removeUserField {
    my $self = shift;
    my($app, $userID, $fieldID) = @_;
    $self->notImplemented();
}

sub setUserGroups {
    my $self = shift;
    my($app, $userID, @groupIDs) = @_;
    $self->notImplemented();
}

sub addUserGroup {
    my $self = shift;
    my($app, $userID, $groupID) = @_;
    $self->notImplemented();
}

sub removeUserGroup {
    my $self = shift;
    my($app, $userID, $groupID) = @_;
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
    $self->notImplemented();
    # return [type, fieldID, category, name, typeData]
}

sub getFieldByName {
    my $self = shift;
    my($app, $category, $name) = @_;
    $self->notImplemented();
    # return [type, fieldID, category, name, typeData]
}

sub getFieldNamesByCategory {
    my $self = shift;
    my($app, $category) = @_;
    $self->notImplemented();
    # return [name, name, name, name ...]
}

sub setField {
    my $self = shift;
    my($app, $fieldID, $category, $name, $type, $data) = @_;
    # if fieldID is undefined, then add a new entry and return the
    # fieldID. Typically data will be undefined then too.
    $self->notImplemented();
    # The caller should make sure that the relevant service
    # ('user.field.$type.manager') is then notified
    # ('fieldAdded($fieldID)') so that any additional database setup
    # can be performed. If this was merely a change and not a new
    # addition, then call fieldChanged($fieldID) instead. (It is a
    # change if you pass $fieldID, and it is an addition if $fieldID
    # is undefined.)
    # returns the fieldID.
}

sub removeField {
    my $self = shift;
    my($app, $fieldID) = @_;
    # The caller should make sure that the relevant service
    # ('user.field.$type.manager') is notified
    # ('fieldRemoved($fieldID)') so that any additional database
    # cleanup can be performed.
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
    $self->notImplemented();
}
