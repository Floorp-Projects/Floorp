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

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'user.factory' or $class->SUPER::provides($service));
}

sub getUserByCredentials {
    my $self = shift;
    my($username, $password) = @_;
    my $object = $self->getUserByUsername($username);
    if ($object->checkPassword($password)) {
        return $object;
    } else {
        return undef;
    }
}

sub getUserByUsername {
    my $self = shift;
    my($username) = @_;
    # XXX
    return undef;
}

sub getUserByID {
    my $self = shift;
    my($id) = @_;
    # XXX
    return undef;
}

sub objectProvides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'user' or $class->SUPER::objectProvides($service));
}

sub objectInit {
    my $self = shift;
    my($app, $userID, $disabled, $password, $adminMessage, $newFieldID, $newFieldValue, $newFieldPassword, $fields, $groups, $rights) = @_;
    $self->SUPER::objectInit(@_);
    $self->userID($userID);
    $self->disabled($disabled);
    $self->password($password);
    $self->adminMessage($adminMessage);
    $self->newFieldID($newFieldID);
    $self->newFieldValue($newFieldValue);
    $self->newFieldPassword($newFieldPassword);
    $self->fields({});
    my $fieldFactory = $self->app->getService('user.field.factory');
    foreach my $fieldID (keys($fields)) {
        my $field = $fieldFactory->createField($app, $self, $fieldID, $fields->{$fieldID});
        $self->fields->{$field->category}->{$field->name} = $field;
    }
    $self->groups({%$groups}); # hash of groupID => groupName
    $self->rights(map {$_ => 1} @$rights); # map a list of strings into a hash for easy access
    $self->{'_DIRTY'} = {};
}

sub hasRight {
    my $self = shift;
    my($right) = @_;
    return defined($self->rights->{$right});
}

sub getField {
    my $self = shift;
    my($type, $name) = @_;
    if (defined($self->fields->{$type})) {
        return $self->fields->{$type}->{$name};
    }
    return undef;
}

sub getAddress {
    # Contact methods should generate usernames (for their 'username'
    # member function) with the syntax "ServiceName: username" e.g.,
    # my AIM username would be "AIM: HixieDaPixie". Services are fully
    # allowed to make an exception to this if they have very
    # distinguishable username syntaxes, for example e-mail addresses
    # should be returned "raw", as in "ian@hixie.ch" and not "E-MAIL:
    # ian@hixie.ch".
    my $self = shift;
    my($protocol) = @_;
    my $field = $self->getField('contact', $protocol);
    if (defined($field)) {
        return $field->data;
    } else {
        return undef;
    }
}

sub prepareAddressChange {
    my $self = shift;
    my($field, $newAddress, $password) = @_;
    $self->newFieldID($field->fieldID);
    $self->newFieldValue($newAddress);
    $self->newFieldPassword($password);
    return $self->objectCreate($self->app, $self->userID, $self->disabled, $self->adminMessage, 
                               $self->newFieldID, $self->newFieldValue, $self->newFieldPassword,
                               {$field->FieldID => $newAddress}, $self->{'groups'}, keys(%{$self->rights}));
}

sub doAddressChange {
    my $self = shift;
    my($password) = @_;
    if ($self->app->getService('service.passwords')->checkPassword($self->newFieldPassword, $password)) {
        # XXX change address
    } else {
        # XXX reset fields
        return 0;
    }
}

sub hash {
    my $self = shift;
    return %$self; # XXX should expand fields too
}

sub checkPassword {
    my $self = shift;
    my($password) = @_;
    return $self->app->getService('service.passwords')->checkPassword($self->password, $password);
}

sub joinGroup {
    my $self = shift;
    my($groupID) = @_;
    $self->{'groups'}->{$groupID} = XXX;
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

sub invalidateRights {
    my $self = shift;
    # XXX redo all the rights
}

sub propertySet {
    my $self = shift;
    my $result = $self->SUPER::propertySet(@_);
    $self->{'_DIRTY'}->{'properties'} = 1;
    return $result;
}

sub propertyGet {
    my $self = shift;
    my($name) = @_;
    if ($name eq 'groups') {
        return {%{$self->groups}}; 
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
}

sub writeProperties {
    # XXX
}

sub writeGroups {
    # XXX
}

# fields write themselves out
