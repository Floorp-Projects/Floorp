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
    # the username for a user field is created by appending the 'data'
    # of the user field to the type data of the field description. For
    # example, for the field 'contact.icq', the type data field might
    # contain the string 'ICQ:' and the user field might be '55378571'
    # making the username 'ICQ:55378571'.
    $self->notImplemented();
    # return userID
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
    # return [fieldID, category, name, type, data]*
}

sub getFieldFromID {
    my $self = shift;
    my($app, $fieldID) = @_;
    $self->notImplemented();
    # return [fieldID, category, name, type, data]
}

sub getFieldFromCategoryAndName {
    my $self = shift;
    my($app, $category, $name) = @_;
    $self->notImplemented();
    # return [fieldID, category, name, type, data]
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
    $self->notImplemented();
}

__END__
+-------------------+
| user              |
+-------------------+
| userID         K1 | auto_increment
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
| userID         K1 | points to entries in the table above
| fieldID        K1 | points to entries in the table below
| data              | e.g. "ian@hixie.ch" or "1979-12-27" or an index into another table
+-------------------+

+-------------------+
| userDataTypes     |
+-------------------+
| fieldID        K1 | auto_increment
| category       K2 | e.g. contact, personal, setting [1]
| name           K2 | e.g. sms, homepage, notifications [1]
| type              | e.g. number, string, notifications [2]
| data              | e.g. "SMS", "optional", null
+-------------------+
 [1] used to find the fieldID for a particular category.name combination
 [2] used to find the factory for the relevant user field object

+-------------------+
| userGroupMapping  |
+-------------------+
| userID         K1 | 
| groupID        K1 |
+-------------------+

+-------------------+
| groups            |
+-------------------+
| groupID        K1 |
| name              | user defined name (can be changed)
+-------------------+

+-------------------+
| groupRightsMapping|
+-------------------+
| groupID        K1 |
| rightID        K1 |
+-------------------+

+-------------------+
| rights            |
+-------------------+
| rightID        K1 | implementation detail - not ever passed to other parts of the code
| name           K2 | the internal name for the right, as used by the code
+-------------------+
