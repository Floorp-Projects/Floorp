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

package PLIF::DataSource;
use strict;
use vars qw(@ISA);
use PLIF::Service;
@ISA = qw(PLIF::Service);
1;

sub database {
    my $self = shift;
    my($app) = @_;
    # do we have a cached, checked copy?
    if (defined($self->{'_database'})) {
        # yes, return it
        return $self->{'_database'};
    }
    # no, find the relevant database and return it
    my @databases = $app->getServiceList('database.'.$self->databaseName);
    foreach my $database (@databases) {
        foreach my $type ($self->databaseType) {
            if ($type eq $database->type) {
                $self->{'_database'} = $database;
                return $database;
            }
        }
    }
    $self->error(1, 'There is no suitable \''.$self->databaseName.'\' database installed.');
}

sub helper {
    my $self = shift;
    my($app) = @_;
    # do we have a cached, checked copy?
    if (defined($self->{'_helper'})) {
        # yes, return it
        return $self->{'_helper'};
    }
    # no, find the relevant database helper and return it
    my @helpers = $app->getServiceList('database.helper');
    foreach my $helper (@helpers) {
        foreach my $helperType ($helper->databaseType) {
            foreach my $sourceType ($self->databaseType) {
                if ($helperType eq $sourceType) {
                    $self->{'_helper'} = $helper;
                    return $helper;
                }
            }
        }
    }
    $self->error(1, 'Configuration Error: There is no database helper suitable for the \''.$self->databaseName.'\' database installed.');
}

sub databaseName {
    my $self = shift;
    $self->notImplemented();
}

sub databaseType {
    my $self = shift;
    $self->notImplemented();
}
