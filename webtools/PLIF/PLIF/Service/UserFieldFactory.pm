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

package PLIF::Service::UserFieldFactory;
use strict;
use vars qw(@ISA);
use PLIF::Service;
@ISA = qw(PLIF::Service);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'user.fieldFactory' or 
            $service eq 'setup.install' or
            $class->SUPER::provides($service));
}

# Field Factory (Factory for Field Instances)
#
# The factory methods below should return service instances (not
# objects or pointers to services in the controller's service
# list!). These service instances should provide the 'user.field.X'
# service where 'X' is a field type. The field type should be
# determined from the fieldID or fieldCategory.fieldName identifiers
# passed to the factory methods.

# typically used when the data comes from the database
sub createFieldByID {
    my $self = shift;
    my($app, $user, $fieldID, $fieldData) = @_;
    my($type, @data) = $app->getService('dataSource.user')->getFieldByID($app, $fieldID);
    $app->assert(defined($type), 1, "Tried to create a field with ID '$fieldID' but that field ID is not defined");
    my $field = $app->getServiceInstance("user.field.$type", $user, @data, $fieldData);
    $app->assert(defined($field), 1, "Tried to create a field of type '$type' but there is no service providing that type");
    return $field;
}

# typically used when the field is being created
sub createFieldByName {
    my $self = shift;
    my($app, $user, $fieldCategory, $fieldName, $fieldData) = @_;
    # fieldData is likely to be undefined, as the field is unlikely to
    # exist for this user.
    my($type, @data) = $app->getService('dataSource.user')->getFieldByName($app, $fieldCategory, $fieldName);
    $app->assert(defined($type), 1, "Tried to create a field with name '$fieldCategory.$fieldName' but that field is not defined");
    my $field = $app->getServiceInstance("user.field.$type", $user, @data, $fieldData);
    $app->assert(defined($field), 1, "Tried to create a field of type '$type' but there is no service providing that type");
    return $field;
}


# Field Factory (Factory for Field Types)
#
# These methods add (and remove) field types to (and from) the
# database. They don't return anything in particular.
# These methods are not expected to be called during normal
# operations, only during installations and upgrades. 

sub registerField {
    my $self = shift;
    my($app, $fieldCategory, $fieldName, $fieldType, @data) = @_;
    my $dataSource = $app->getService('dataSource.user');
    # see if the field already exists, so that we can keep the fieldID
    # the same:
    my($oldType, $fieldID) = $dataSource->getFieldByName($app, $fieldCategory, $fieldName);
    # if the type changes, then act as if it was deleted:
    if ((defined($oldType)) and ($oldType ne $fieldType)) {
        $app->getCollectingServiceList("user.field.$oldType.manager")->fieldRemoved($fieldID);
    }
    $fieldID = $dataSource->setField($app, $fieldID, $fieldCategory, $fieldName, $fieldType, @data);
    $self->assert(defined($fieldID), 1, "Call to 'setField' for field '$fieldCategory.fieldType' of type $fieldType failed to return a valid field ID.");
    # if the field is new or if the type changed, then notify the field type's manager of this:
    if ((not defined($oldType)) or ($oldType ne $fieldType)) {
        $app->getCollectingServiceList("user.field.$fieldType.manager")->fieldAdded($fieldID);
    } else {
        # otherwise, just do a change notification
        $app->getCollectingServiceList("user.field.$fieldType.manager")->fieldChanged($fieldID);
    }
    # return the fieldID
    return $fieldID;
}

sub registerSetting {
    my $self = shift;
    my($app, $setting, @data) = @_;
    return $self->registerField($app, 'setting', $setting, @data);
}

sub removeField {
    my $self = shift;
    my($app, $fieldCategory, $fieldName) = @_;
    my $dataSource = $app->getService('dataSource.user');
    # get the field's data (ID and type, in particular)
    my($fieldType, $fieldID) = $dataSource->getFieldByName($app, $fieldCategory, $fieldName);
    if (defined($fieldType)) {
        $app->getCollectingService("user.field.$fieldType.manager")->fieldRemoved($fieldID);
        $dataSource->removeField($app, $fieldID);
    } # else, field wasn't there to start with, so...
    return $fieldID;
}


# setup.install
sub setupInstall {
    my $self = shift;
    my($app) = @_;
    $app->output->setupProgress('user fields');
    $app->getCollectingServiceList('user.fieldRegisterer')->register($app, $self);
    return;
}
