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

package PLIF::Service::User;
use strict;
use vars qw(@ISA);
use PLIF::Service::Session;
@ISA = qw(PLIF::Service::Session);
1;

# This class implements an object and its associated factory service.
# Compare this with the UserFieldFactory class which implements a
# factory service only, and the various UserField descendant classes
# which implement Service Instances.

# XXX It would be interesting to implement crack detection (time since
# last incorrect login, number of login attempts performed with time
# since last incorrect login < a global delta, address changing
# timeout, etc).

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'user.factory' or $class->SUPER::provides($service));
}

__DATA__

sub getUserByCredentials {
    my $self = shift;
    my($app, $username, $password) = @_;
    my $object = $self->getUserByUsername($app, $username);
    if (defined($object) and ($object->checkPassword($password))) {
        return $object;
    } else {
        return undef;
    }
}

sub getUserByUsername {
    my $self = shift;
    my($app, $username) = @_;
    my(@data) = $app->getService('dataSource.user')->getUserByUsername($app, $username);
    if (@data) {
        return $self->objectCreate($app, @data);
    } else {
        return undef;
    }
}

sub getUserByContactDetails {
    my $self = shift;
    my($app, $contactName, $address) = @_;
    my(@data) = $app->getService('dataSource.user')->getUserByContactDetails($app, $contactName, $address);
    if (@data) {
        return $self->objectCreate($app, @data);
    } else {
        return undef;
    }
}

sub getUserByID {
    my $self = shift;
    my($app, $userID) = @_;
    # do we already have that user?
    my $user = $app->getObject("user.$userID");
    if (defined($user)) {
        # got a user created already, return that
        return $user;
    } else {
        # that user isn't in our system yet, look it up
        my(@data) = $app->getService('dataSource.user')->getUserByID($app, $userID);
        if (@data) {
            return $self->objectCreate($app, @data);
        } else {
            return undef;
        }
    }
}

sub getNewUser {
    my $self = shift;
    my($app, $password) = @_;
    return $self->objectCreate($app, undef, 0, $password, '', {}, [], []);
}

sub objectProvides {
    my $self = shift;
    my($service) = @_;
    return ($service eq 'user' or
            $service eq 'user.'.($self->{userID}) or
            $self->SUPER::objectProvides($service));
}

# We don't need this yet, so it's commented out as an optimisation.
# # Override the default constructor to check to see if the user already
# # has an object associated with them in the system, and if they do,
# # use that instead of creating a new one.
# sub objectCreate {
#     my $class = shift;
#     my($app, $userID, @data) @_;
#     return $app->getObject("user.$userID") || $class->SUPER::objectCreate($app, $userID, @data);
#     # more strictly:
#     #
#     # my $user = $app->getObject("user.$userID");
#     # if (not defined($user)) {
#     #     $user = $class->SUPER::objectCreate($app, $userID, @data);
#     # }
#     # return $user;
# }

sub objectInit {
    my $self = shift;
    my($app, $userID, $mode, $password, $adminMessage, $fields, $groups, $rights) = @_;
    $self->{'_DIRTY'} = {}; # make sure propertySet is happy
    $self->SUPER::objectInit(@_);
    $self->{userID} = $userID;
    $self->{mode} = $mode; # 0=active, 1=disabled   XXX need a way to make this extensible
    $self->{password} = $password;
    $self->{adminMessage} = $adminMessage;
    $self->{fields} = {};
    $self->{fieldsByID} = {};
    # don't forget to update the 'hash' function if you add more properties/field whatever you want to call them
    my $fieldFactory = $app->getService('user.fieldFactory');
    foreach my $fieldID (keys(%$fields)) {
        $self->insertField($fieldFactory->createFieldByID($app, $self, $fieldID, $fields->{$fieldID}));
    }
    # $groups is an array of arrays containing groupID, groupName, user level in group
    my $groupsByID = {};
    my $groupsByName = {};
    foreach my $group (@$groups) {
        $groupsByID->{$group->[0]} = {'name' => $group->[1], 'level' => $group->[2], }; # id => name, level
        $groupsByName->{$group->[1]} = {'groupID' => $group->[0], 'level' => $group->[2], }; # name => id, level
    }
    $self->{groupsByID} = $groupsByID; # authoritative version
    $self->{originalGroupsByID} = {%{$groupsByID}}; # a backup used to make a comparison when saving the groups
    $self->{groupsByName} = $groupsByName; # helpful version for output purposes only
    # rights
    $self->{rights} = { map {$_ => 1} @$rights }; # map a list of strings into a hash for easy access
    $self->{'_DIRTY'}->{'properties'} = not(defined($userID));
}

sub hasRight {
    my $self = shift;
    my($right) = @_;
    return (defined($self->{rights}->{$right}) or $self->levelInGroup(1)); # group 1 is a magical group
}

sub hasField {
    my $self = shift;
    my($category, $name) = @_;
    if (defined($self->{fields}->{$category})) {
        return $self->{fields}->{$category}->{$name};
    }
    return undef;
}

# returns a field *even if it does not exist yet*
# -- so if you want to add a field by name, you do:
#     $user->getField('category', 'name')->data($myData);
sub getField {
    my $self = shift;
    my($category, $name) = @_;
    my $field = $self->hasField($category, $name);
    if (not defined($field)) {
        $field = $self->insertField($self->{app}->getService('user.fieldFactory')->createFieldByName($self->{app}, $self, $category, $name));
    }
    return $field;
}

# returns a field *even if it does not exist yet*
# -- so if you want to add a field by ID, you do:
#     $user->getField($fieldID)->data($myData);
sub getFieldByID {
    my $self = shift;
    my($ID) = @_;
    my $field = $self->{fieldsByID}->{$ID};
    if (not defined($field)) {
        $field = $self->insertField($self->{app}->getService('user.fieldFactory')->createFieldByID($self->{app}, $self, $ID));
    }
    return $field;
}

sub getAddress {
    my $self = shift;
    my($protocol) = @_;
    my $field = $self->hasField('contact', $protocol);
    if (defined($field)) {
        return $field->address;
    } else {
        return undef;
    }
}

sub addFieldChange {
    my $self = shift;
    my($field, $newData, $password, $type) = @_;
    $field->prepareChange($newData);
    return $self->{app}->getService('dataSource.user')->setUserFieldChange($self->{app}, $self->{userID}, $field->{fieldID}, $newData, $password, $type);
}

sub performFieldChange {
    my $self = shift;
    my($changeID, $candidatePassword, $minTime) = @_;
    my $dataSource = $self->{app}->getService('dataSource.user');
    my($userID, $fieldID, $newData, $password, $createTime, $type) = $dataSource->getUserFieldChangeFromChangeID($self->{app}, $changeID);
    # check for valid change
    if ((not defined($userID)) or # invalid change ID
        ($userID != $self->{userID}) or # wrong change ID
        (not $self->{app}->getService('service.password')->checkPassword($candidatePassword, $password)) or # wrong password
        ($createTime < $minTime)) { # expired change
        return 0;
    }
    # perform the change
    $self->getFieldByID($fieldID)->data($newData);
    # remove the change from the list of pending changes
    if ($type == 1) { # XXX HARDCODED CONSTANT ALERT
        # this is an override change
        # remove all pending changes for this field (including this one)
        $dataSource->removeUserFieldChangesByUserIDAndFieldID($self->{app}, $userID, $fieldID);
    } else {
        # this is a normal change
        # remove just this change
        $dataSource->removeUserFieldChangesByChangeID($self->{app}, $changeID);
    }
    return 1;
}

# a convenience method for either setting a user setting from a new
# value or getting the user's prefs
sub setting {
    my $self = shift;
    my($variable, $setting) = @_;
    $self->assert(ref($variable) eq 'SCALAR', 1, 'Internal Error: User object was expecting a scalar ref for setting() but didn\'t get one');
    if (defined($$variable)) {
        $self->getField('settings', $setting)->data($$variable);
    } else {
        my $field = $self->hasField('settings', $setting);
        if (defined($field)) {
            $$variable = $field->data;
        }
    }
}

sub hash {
    my $self = shift;
    my $result = $self->SUPER::hash();
    $result->{'userID'} = $self->{userID},
    $result->{'mode'} = $self->{mode},
    $result->{'adminMessage'} = $self->{adminMessage},
    $result->{'groupsByID'} = $self->{groupsByID};
    $result->{'groupsByName'} = $self->{groupsByName};
    $result->{'rights'} = [keys(%{$self->{rights}})];
    if ($self->levelInGroup(1)) {
        # has all rights
        $result->{'right'} = {};
        foreach my $right (@{$self->{app}->getService('dataSource.user')->getAllRights($self->{app})}) {
            $result->{'right'}->{$right} = 1;
        }
    } else {
        $result->{'right'} = $self->{rights};
    }
    $result->{'fields'} = {};
    foreach my $field (values(%{$self->{fieldsByID}})) {
        # XXX should we also pass the field metadata on? (e.g. typeData)
        $result->{'fields'}->{$field->{fieldID}} = $field->hash; # (not an array btw: could have holes)
        $result->{'fields'}->{$field->{category} . '.' . $field->{name}} = $field->hash;
    }
    return $result;
}

sub checkPassword {
    my $self = shift;
    my($password) = @_;
    return $self->{app}->getService('service.passwords')->checkPassword($self->{password}, $password);
}

sub checkLogin {
    my $self = shift;
    return ($self->{mode} == 0);
}

sub joinGroup {
    my $self = shift;
    my($groupID, $level) = @_;
    if ($level > 0) {
        my $groupName = $self->{app}->getService('dataSource.user')->getGroupName($self->{app}, $groupID);
        $self->{'groupsByID'}->{$groupID} = {'name' => $groupName, 'level' => $level, };
        $self->{'groupsByName'}->{$groupName} = {'groupID' => $groupID, 'level' => $level, };
        $self->invalidateRights();
        $self->{'_DIRTY'}->{'groups'} = 1;
    } else {
        $self->leaveGroup($groupID, $level);
    }
}

sub leaveGroup {
    my $self = shift;
    my($groupID) = @_;
    if (defined($self->{'groupsByID'}->{$groupID})) {
        delete($self->{'groupsByName'}->{$self->{'groupsByID'}->{$groupID}->{'name'}});
        delete($self->{'groupsByID'}->{$groupID});
        $self->invalidateRights();
        $self->{'_DIRTY'}->{'groups'} = 1;
    }
}

sub levelInGroup {
    my $self = shift;
    my($groupID) = @_;
    if (defined($self->{'groupsByID'}->{$groupID})) {
        return $self->{'groupsByID'}->{$groupID}->{'level'};
    } else {
        return 0;
    }
}


# internal routines

sub insertField {
    my $self = shift;
    my($field) = @_;
    $self->assert(ref($field) and $field->provides('user.field'), 1, 'Tried to insert something that wasn\'t a field object into a user\'s field hash');
    $self->{fields}->{$field->{category}}->{$field->{name}} = $field;
    $self->{fieldsByID}->{$field->{fieldID}} = $field;
    return $field;
}

sub invalidateRights {
    my $self = shift;
    my $rights = $self->{app}->getService('dataSource.user')->getRightsForGroups($self->{app}, keys(%{$self->{'groupsByID'}}));
    $self->{rights} = { map {$_ => 1} @$rights }; # map a list of strings into a hash for easy access
    # don't set a dirty flag, because rights are merely a convenient
    # cached expansion of the rights data. Changing this externally
    # makes no sense -- what rights one has is dependent on what
    # groups one is in, and changing the rights won't magically change
    # what groups you are in (how could it).
}

sub propertySet {
    my $self = shift;
    my($name, $value) = @_;
    # check that we're not doing silly things like changing the user's ID
    my $hadUndefinedID = (($name eq 'userID') and
                          ($self->propertyExists($name)) and
                          (not defined($self->propertyGet($name))));
    my $result = $self->SUPER::propertySet(@_);
    if (($hadUndefinedID) and (defined($value))) {
        # we've just aquired an ID, so propagate the change to all fields
        foreach my $field (values(%{$self->{fieldsByID}})) {
            $field->{userID} = $value;
        }
        # and mark the groups as dirty too
        $self->{'_DIRTY'}->{'groups'} = 1;
    }
    $self->{'_DIRTY'}->{'properties'} = 1;
    return $result;
}

sub propertyGet {
    my $self = shift;
    my($name) = @_;
    if ($name eq 'groupsByID') {
        return {%{$self->{'groupsByID'}}};
        # Create new hash so that they can't edit ours. This ensures
        # that they can't inadvertently bypass the DIRTY flagging by
        # propertySet(), above. This does mean that internally we have
        # to access $self->{'groupsByID'} instead of $self->{groupsByID}.
    } else {
        # we don't bother looking at $self->rights or
        # $self->{groupsByName}, but any changes made to those won't be
        # saved anyway.
        return $self->SUPER::propertyGet(@_);
    }
}

sub implyMethod {
    my $self = shift;
    my($name, @data) = @_;
    if (@data == 1) {
        return $self->propertySet($name, @data);
    } elsif (@data == 0) {
        return $self->propertyGet($name);
    } else {
        return $self->SUPER::implyMethod(@_);
    }
}

sub DESTROY {
    my $self = shift;
    if ($self->{'_DIRTY'}->{'properties'}) {
        $self->writeProperties();
    }
    if ($self->{'_DIRTY'}->{'groups'}) {
        $self->writeGroups();
    }
}

sub writeProperties {
    my $self = shift;
    $self->{userID} = $self->{app}->getService('dataSource.user')->setUser($self->{app}, $self->{userID}, $self->{mode},
                                                                           $self->{password}, $self->{adminMessage},
                                                                           $self->newFieldID, $self->{newFieldValue}, $self->{newFieldKey});
}

sub writeGroups {
    my $self = shift;
    # compare the group lists before and after and see which got added or changed and which got removed
    my $dataSource = $self->{app}->getService('dataSource.user');
    foreach my $group (keys(%{$self->{'groupsByID'}})) {
        if ((not defined($self->{'originalGroupsByID'}->{$group})) or
            ($self->{'groupsByID'}->{$group}->{'level'} != $self->{'originalGroupsByID'}->{$group}->{'level'})) {
            $dataSource->addUserGroup($self->{app}, $self->{userID}, $group, $self->{'groupsByID'}->{$group}->{'level'});
        }
    }
    foreach my $group (keys(%{$self->{'originalGroupsByID'}})) {
        if (not defined($self->{'groupsByID'}->{$group})) {
            $dataSource->removeUserGroup($self->{app}, $self->{userID}, $group);
        }
    }
}

# fields write themselves out
