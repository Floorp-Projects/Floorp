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

package PLIF::Application;
use strict;
use vars qw(@ISA);
@ISA = qw(PLIF::Program);
require PLIF::Program; # see note below 
1;

# Note: this module "require"s PLIF::Program (as opposed to "use"ing
# it) because that module will try to call 'main'->create, which won't
# work if the module is parsed during compilation instead of during
# execution. For the same reason, the @ISA line is above the
# require. All modules that have PLIF::Application as an ancestor need
# to do this.
#
# In theory, if you use PLIF::Application, the class tree should look
# like this:
# 
#    PLIF                (the core module)
#     |
#    PLIF::Controller    (defines the service management)
#     |
#    PLIF::Program       (defines things like 'input' and 'output')
#     |
#    PLIF::Application   (defines the generic command dispatcher)
#     |
#    A PLIF Shell        (bootstraps PLIF::Application)
#
# However, you might want to skip the PLIF::Application layer if all
# you are doing is writing a `simple' utility (although frankly I
# would doubt your choice of PLIF as an infrastructure if all you are
# looking for is a `simple' utility -- HTTP content negotiation and
# database-agnostic logic is probably a bit of an overkill there...).
#
# If you are writing an application that uses PLIF for some part of
# the work, but not for input and output, then you would probably
# inherit straight from PLIF::Controller, and only use the getService
# call (and friends).

# find either a service or a one-shot object that claims to implement
# command dispatching, and ask them to handle this.
sub dispatch {
    my $self = shift;
    my($command) = @_;
    if (not ($self->getSelectingObjectList('commands.dispatcher')->dispatch($self, $command) or 
             $self->getSelectingObjectList('commands.dispatcher')->dispatch($self, $command))) {
        $self->SUPER::dispatch(@_);
    }
}

sub registerServices {
    my $self = shift;
    $self->SUPER::registerServices(@_);
    $self->registerDefaultServices();
    $self->registerInstalledServices();
}

sub registerDefaultServices {
    my $self = shift;
    # install the configuration system
    $self->register(qw(PLIF::DataSource::Configuration PLIF::Database::ConfigurationFile));
}

sub registerInstalledServices {
    my $self = shift;
    # install the modules from the configuration database
    my $modules = $self->getService('dataSource.configuration')->getInstalledModules($self);
    if (defined($modules)) {
        $self->register(@$modules);
    }
}
