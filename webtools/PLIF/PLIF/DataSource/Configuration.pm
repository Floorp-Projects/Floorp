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

package PLIF::DataSource::Configuration;
use strict;
use vars qw(@ISA);
use PLIF::DataSource;
@ISA = qw(PLIF::DataSource);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'dataSource.configuration' or $service eq 'setup.configure' or $class->SUPER::provides($service));
}

sub databaseName {
    return 'configuration';
}

sub databaseType {
    return 'property';
}


# Configuration API Implementation Follows

sub configurationFilename {
    return '.PLIF';
}

sub setupConfigure {
    my $self = shift;
    my($app) = @_;
    $self->dump(9, 'about to configure Configuration data source...');
    eval {
        # if it failed earlier but without crashing the app, then it
        # will fail again (we only stop trying once it works)
        $self->database($app)->ensureRead();
    };
    if ($@) {
        # well, that didn't go down too well. Let's create a brand
        # spanking new configuration file, since they clearly don't
        # have one.
        $app->output->setupProgress('configuration');
        $self->database($app)->assumeRead(); # new file at the ready
        # options should now be set by the users of the datasource.
    }
    $self->dump(9, 'done configuring Configuration data source');
    return; # no problems
}

sub getInstalledModules {
    my $self = shift;
    my($app) = @_;
    return $self->database($app)->propertyGet('PLIF.modulesList');
}

sub setInstalledModules {
    my $self = shift;
    my($app, $value) = @_;
    $self->database($app)->propertySet('PLIF.modulesList', $value);
}

sub getDBIDatabaseSettings {
    my $self = shift;
    my($app, $database) = @_;
    my $configuration = $self->database($app);
    my $prefix = 'database.'.$database->class;
    foreach my $property ($database->settings) {
        my $value = $configuration->propertyGet("$prefix.$property");
        $self->assert($value, 1, "The configuration is missing a valid value for '$prefix.$property'");
        $database->propertySet($property, $value);
    }
}

sub setDBIDatabaseSettings {
    my $self = shift;
    my($app, $database) = @_;
    my $configuration = $self->database($app);
    my $prefix = 'database.'.$database->class;
    foreach my $property ($database->settings) {
        $configuration->propertySet("$prefix.$property", $database->propertyGet($property));
    }
}

# this takes an object supporting the dataSource.configuration.client
# service and retrieves its settings.
sub getSettings {
    my $self = shift;
    my($app, $object, $prefix) = @_;
    my $configuration = $self->database($app);
    foreach my $property ($object->settings) {
        my $value = $configuration->propertyGet("$prefix.$property");
        $self->assert($value, 1, "The configuration is missing a valid value for '$prefix.$property'");
        $object->propertySet($property, $value);
    }
}

# this takes an object supporting the dataSource.configuration.client
# service and saves its settings.
sub setSettings {
    my $self = shift;
    my($app, $object, $prefix) = @_;
    my $configuration = $self->database($app);
    foreach my $property ($object->settings) {
        $configuration->propertySet("$prefix.$property", $object->propertyGet($property));
    }
}
