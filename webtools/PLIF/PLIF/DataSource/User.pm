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
    return $self->getUserByID($app, $self->getUserIDByUsername($app, $username));
    # return the same as getUserByID()
}

sub getUserIDByUsername {
    my $self = shift;
    my($app, $username) = @_;
    $self->notImplemented();
    # return userID
}

sub getUserByID {
    my $self = shift;
    my($app, $id) = @_;
    $self->notImplemented();
    # return userID, disabled, password, adminMessage, newFieldID, newFieldValue, 
    # newFieldKey, { fieldID => data }*, { groupID => name }*, [rightNames]*
}

sub setUser {
    my $self = shift;
    my($app, $userID, $disabled, $password, $adminMessage, $newFieldID, $newFieldValue, $newFieldKey) = @_;
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

sub setUserGroups
    my $self = shift;
    my($app, $userID, @groupIDs) = @_;
    $self->notImplemented();
}

sub getFields {
    my $self = shift;
    my($app) = @_;
    $self->notImplemented();
    # return [fieldID, category, name, type, data]*
}

sub setField {
    my $self = shift;
    my($app, $fieldID, $category, $name, $type, $data) = @_;
    # if fieldID is undefined, then add a new entry and return the
    # fieldID.
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
    my($app, $groupID, @rightNames) = @_;
    # if groupID is undefined, yada yada.
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

__END__
+-------------------+
| user              |
+-------------------+
| userID            | auto_increment
| password          |
| disabled          | boolean
| adminMessage      | string displayed when user (tries to) log in
| newFieldID        | \
| newFieldValue     |  > used when user tries to change his e-mail
| newFieldKey       | /  address, for example
+-------------------+

+-------------------+
| userData          |
+-------------------+
| userID            | points to entries in the table above
| fieldID           | points to entries in the table below
| data              | e.g. "ian@hixie.ch" or "1979-12-27" or an index into another table
+-------------------+

+-------------------+
| userDataTypes     |
+-------------------+
| fieldID           | auto_increment
| category          | e.g. contact, personal, setting
| name              | e.g. sms, homepage, notifications
| type              | e.g. number, string, notifications
| data              | e.g. "[0-9- ]*", "uri", null
+-------------------+

+-------------------+
| userGroupMapping  |
+-------------------+
| userID            | 
| groupID           |
+-------------------+

+-------------------+
| groups            |
+-------------------+
| groupID           |
| name              | user defined name (can be changed)
+-------------------+

+-------------------+
| groupRightsMapping|
+-------------------+
| groupID           |
| rightID           |
+-------------------+

+-------------------+
| rights            |
+-------------------+
| rightID           | implementation detail - not ever passed to other parts of the code
| name              | the internal name for the right, as used by the code
+-------------------+
