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
    my(@data) = $app->getService('dataSource.user')->getUserByUserID($app, $userID);
    if (@data) {
        return $self->objectCreate($app, @data);
    } else {
        return undef;
    }
}

sub getNewUser {
    my $self = shift;
    my($app, $password) = @_;
    return $self->objectCreate($app, undef, 0, $password, '', '', '', '', {}, {}, []);
}

sub objectProvides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'user' or $class->SUPER::objectProvides($service));
}

sub objectInit {
    my $self = shift;
    my($app, $userID, $mode, $password, $adminMessage, $newFieldID, $newFieldValue, $newFieldPassword, $fields, $groups, $rights) = @_;
    $self->{'_DIRTY'} = {}; # make sure propertySet is happy
    $self->SUPER::objectInit(@_);
    $self->userID($userID);
    $self->mode($mode); # active (0), disabled (1), logging out (2) XXX need a way to make this extensible
    $self->password($password);
    $self->adminMessage($adminMessage);
    $self->newFieldID($newFieldID);
    $self->newFieldValue($newFieldValue);
    $self->newFieldPassword($newFieldPassword);
    $self->fields({});
    $self->fieldsByID({});
    # don't forget to update the 'hash' function if you add more fields
    my $fieldFactory = $app->getService('user.fieldFactory');
    foreach my $fieldID (keys(%$fields)) {
        $self->insertField($fieldFactory->createFieldByID($app, $self, $fieldID, $fields->{$fieldID}));
    }
    $self->groups({%$groups}); # hash of groupID => groupName
    $self->originalGroups({%$groups}); # a backup used to make a comparison when saving the groups
    my %rights = map {$_ => 1} @$rights;
    $self->rights(\%rights); # map a list of strings into a hash for easy access
    $self->{'_DIRTY'}->{'properties'} = not(defined($userID));
}

sub hasRight {
    my $self = shift;
    my($right) = @_;
    return defined($self->rights->{$right});
}

sub hasField {
    my $self = shift;
    my($category, $name) = @_;
    if (defined($self->fields->{$category})) {
        return $self->fields->{$category}->{$name};
    }
    return undef;
}

# if you want to add a field, you do:
#     $user->getField('category', 'name')->data($myData);
sub getField {
    my $self = shift;
    my($category, $name) = @_;
    my $field = $self->hasField($category, $name);
    if (not defined($field)) {
        $field = $self->insertField($self->app->getService('user.fieldFactory')->createFieldByName($self->app, $self, $category, $name));
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

sub prepareAddressChange {
    my $self = shift;
    my($field, $newAddress, $password) = @_;
    if ($field->validate($newAddress)) {
        $self->newFieldID($field->fieldID);
        $self->newFieldValue($newAddress);
        $self->newFieldPassword($password);
        $field->prepareAddressChange($newAddress);
        return $self;
    } else {
        return undef;
    }
}

sub prepareAddressAddition {
    my $self = shift;
    my($fieldName, $newAddress, $password) = @_;
    my $field = $self->insertField($self->app->getService('user.fieldFactory')->createFieldByName($self->app, $self, 'contact', $fieldName, undef));
    return $self->prepareAddressChange($field, $newAddress, $password);
}

sub doAddressChange {
    my $self = shift;
    my($password) = @_;
    # it is defined that if $password is undefined, then this method
    # will reset the fields to prevent multiple attempts. See the
    # resetAddressChange() method.
    if ($self->newFieldID) {
        my $field = $self->fieldsByID->{$self->newFieldID};
        $self->assert(defined($field), 1, 'Database integrity error: newFieldID doesn\'t map to a field!');
        if (defined($password) and ($self->app->getService('service.passwords')->checkPassword($self->newFieldPassword, $password))) {
            $field->data($self->newFieldValue);
        } elsif (not defined($field->data)) {
            $field->remove();
        }
        $self->newFieldID(undef);
        $self->newFieldValue(undef);
        $self->newFieldPassword(undef);
        return $field;
    } else {
        return 0;
    }
}

sub resetAddressChange {
    my $self = shift;
    # calling the doAddressChange() function with no arguments is the
    # same as calling doAddressChange() with the wrong password, which
    # resets the fields (to prevent multiple attempts)
    return defined($self->doAddressChange());
}

# a convenience method for either setting a user setting from a new
# value or getting the user's prefs
sub setting {
    my $self = shift;
    my($variable, $setting) = @_;
    $self->assert(ref($variable) eq 'SCALAR', 1, 'Internal Error: User object was expecting a scalar ref for setting() but didn\'t get one');
    if (defined($$variable)) {
        $self->getField('setting', $setting)->data($$variable);
    } else {
        my $field = $self->hasField('setting', $setting);
        if (defined($field)) {
            $$variable = $field->data;
        }
    }
}

sub hash {
    my $self = shift;
    my $result = $self->SUPER::hash();
    $result->{'userID'} = $self->userID,
    $result->{'mode'} = $self->mode,
    $result->{'adminMessage'} = $self->adminMessage,
    $result->{'groups'} = $self->groups;
    $result->{'rights'} = [keys(%{$self->rights})];
    $result->{'right'} = $self->rights;
    $result->{'fields'} = {};
    foreach my $field (values(%{$self->fieldsByID})) {
        # XXX should we also pass the field metadata on? (e.g. typeData)
        $result->{'fields'}->{$field->fieldID} = $field->data; # (not an array btw)
        $result->{'fields'}->{$field->category.':'.$field->name} = $field->data;
    }
    return $result;
}

sub checkPassword {
    my $self = shift;
    my($password) = @_;
    return $self->app->getService('service.passwords')->checkPassword($self->password, $password);
}

sub joinGroup {
    my $self = shift;
    my($groupID) = @_;
    $self->{'groups'}->{$groupID} = $self->app->getService('dataSource.user')->getGroupName($self->app, $groupID);
    $self->invalidateRights();
    $self->{'_DIRTY'}->{'groups'} = 1;
}

sub leaveGroup {
    my $self = shift;
    my($groupID) = @_;
    delete($self->{'groups'}->{$groupID});
    $self->invalidateRights();
    $self->{'_DIRTY'}->{'groups'} = 1;
}


# internal routines

sub insertField {
    my $self = shift;
    my($field) = @_;
    $self->fields->{$field->category}->{$field->name} = $field;
    $self->fieldsByID->{$field->fieldID} = $field;
    return $field;
}

sub invalidateRights {
    my $self = shift;
    my $rights = $self->app->getService('dataSource.user')->getRights($self->app, keys(%{$self->{'groups'}}));
    $self->rights(map {$_ => 1} @$rights); # map a list of strings into a hash for easy access
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
    if (($name eq 'userID') and (defined($value)) and ($hadUndefinedID)) {
        # we've just aquired an ID, so propagate the change to all fields
        foreach my $field (values(%{$self->fieldsByID})) {
            $field->userID($value);
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
    if ($name eq 'groups') {
        return {%{$self->{'groups'}}};
        # Create new hash so that they can't edit ours. This ensures
        # that they can't inadvertently bypass the DIRTY flagging by
        # propertySet(), above. This does mean that internally we have
        # to access $self->{'groups'} instead of $self->groups.
    } else {
        # we don't bother looking at $self->rights, but any changes
        # made to it won't be saved anyway.
        return $self->SUPER::propertyGet(@_);
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
    $self->SUPER::DESTROY(@_);
}

sub writeProperties {
    my $self = shift;
    $self->userID($self->app->getService('dataSource.user')->setUser($self->app, $self->userID, $self->mode,
                                                                     $self->password, $self->adminMessage,
                                                                     $self->newFieldID, $self->newFieldValue, $self->newFieldKey));
}

sub writeGroups {
    my $self = shift;
    # compare the group lists before and after and see which got added and which got removed
    my $dataSource = $self->app->getService('dataSource.user');
    foreach my $group (keys(%{$self->{'groups'}})) {
        if (not defined($self->{'originalGroups'}->{$group})) {
            $dataSource->addUserGroup($self->app, $self->userID, $group);
        }
    }
    foreach my $group (keys(%{$self->{'originalGroups'}})) {
        if (not defined($self->{'groups'}->{$group})) {
            $dataSource->removeUserGroup($self->app, $self->userID, $group);
        }
    }
}

# fields write themselves out
