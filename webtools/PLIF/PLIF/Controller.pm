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

package PLIF::Controller;
use strict;
use vars qw(@ISA);
use PLIF;
use PLIF::MagicPipingArray;
use PLIF::MagicSelectingArray;
use PLIF::MagicCollectingArray;
@ISA = qw(PLIF);
1;

# setup everything (typically called from the constructor)
sub init {
    my $self = shift;
    $self->SUPER::init(@_);
    # initialise our app name to be the name of the executable
    $self->name($0); # may be overridden by descendants
    # prepare the services array for the registration system
    $self->services([]);
    $self->servicesHash({});
    # perform the registration
    $self->registerServices();
}

# should be called from the implementation of registerServices, should
# be passed a list similar to the @ISA list. The order matters, since
# services will be instantiated on a first-matched first-used basis
sub register {
    my $self = shift;
    foreach my $service (@_) {
        push(@{$self->services}, $service);
        my $file = $service;
        # XXX THIS IS PLATFORM SPECIFIC CODE XXX
        if ($^O eq 'linux') {
            $file =~ s/::/\//go;
            $file .= '.pm';
        } else {
            $self->error(0, "Platform '$^O' not supported yet.");
        }
        # XXX END OF PLATFORM SPECIFIC CODE XXX
        eval {
            require $file;
        };
        if ($@) {
            $self->error(1, "Compile error in $file: $@");
        }
    }
}

# helper method for (e.g.) input verifiers to add instantiated service
# objects specific to the current state (e.g. the current user in an
# event loop). These should be wiped out when the state changes (e.g.
# at the start of an event loop).
#
# Objects should be created with $service->createObject(), not with
# $app->getServiceInstance(), because they end up calling different
# constructors -- init() vs objectInit().
sub addObject {
    my $self = shift;
    foreach my $object (@_) {
        push(@{$self->objects}, $object);
    }
}

sub getService {
    my $self = shift;
    my($name) = @_;
    if (defined($self->servicesHash->{$name})) {
        return $self->servicesHash->{$name};
    }
    foreach my $service (@{$self->services}) {
        if ($service->provides($name)) {
            # Create the service. If it is already created, this will
            # just return the object reference, so no harm done.
            # IT IS ABSOLUTELY IMPERATIVE THAT NO SERVICE EVER HOLD ON
            # TO THE $self ARGUMENT PASSED TO THE CONSTRUCTOR!
            # Doing so would create a circular dependency, resulting
            # in a memory leak.
            $service = $service->create($self);
            $self->servicesHash->{$name} = $service;
            return $service;
        }
    }
    return undef;
}

sub getObject {
    # same as getService but on the objects list and without the
    # constructor call
    my $self = shift;
    my($name) = @_;
    foreach my $service (@{$self->objects}) {
        if ($service->objectProvides($name)) {
            return $service;
        }
    }
    return undef;
}

sub getServiceList {
    my $self = shift;
    my($name) = @_;
    my @services = ();
    foreach my $service (@{$self->services}) {
        if ($service->provides($name)) {
            # Create the service. If it is already created, this will
            # just return the object reference, so no harm done.
            # IT IS ABSOLUTELY IMPERATIVE THAT NO SERVICE EVER HOLD ON
            # TO THE $self ARGUMENT PASSED TO THE CONSTRUCTOR!
            # Doing so would create a circular dependency, resulting
            # in a memory leak.
            $service = $service->create($self);
            push(@services, $service);
        }
    }
    local $" = '\', \'';
    $self->dump(10, "Created a service list for '$name' containing: '@services'");
    return @services;
}

sub getObjectList {
    # same as getServiceList but on the objects list and without the
    # constructor call
    my $self = shift;
    my($name) = @_;
    my @services = ();
    foreach my $service (@{$self->objects}) {
        if ($service->objectProvides($name)) {
            push(@services, $service);
        }
    }
    $self->dump(10, "Created an object list for '$name' containing: '@services'");
    return @services;
}

sub getCollectingServiceList {
    my $self = shift;
    return PLIF::MagicCollectingArray->create($self->getServiceList(@_));
}

sub getCollectingObjectList {
    my $self = shift;
    return PLIF::MagicCollectingArray->create($self->getObjectList(@_));
}

sub getPipingServiceList {
    my $self = shift;
    return PLIF::MagicPipingArray->create($self->getServiceList(@_));
}

sub getPipingObjectList {
    my $self = shift;
    return PLIF::MagicPipingArray->create($self->getObjectList(@_));
}

sub getSelectingServiceList {
    my $self = shift;
    return PLIF::MagicSelectingArray->create($self->getServiceList(@_));
}

sub getSelectingObjectList {
    my $self = shift;
    return PLIF::MagicSelectingArray->create($self->getObjectList(@_));
}

sub getServiceInstance {
    my $self = shift;
    my($name, @data) = @_;
    foreach my $service (@{$self->services}) {
        if ($service->provides($name)) {
            # Create and return the service instance, without storing
            # a copy.
            # This is the only time it is safe for a service to store
            # a reference to us. This is because here no reference to
            # the service is being held by us, so the moment the
            # service goes out of scope, it will be freed. 
            # IMPORTANT! DON'T HOLD ON TO A SERVICE INSTANCE OBJECT!
            return $service->serviceCreate($self, @data);
        }
    }
    return undef;
}

# there's no getObjectInstance since objects already are instances...

# What is the difference between a service instance and an object? The
# difference is subtle. Objects are instances of classes that are
# intended to be added to the application's object list, they are
# created by invoking the 'createObject' method on a service, they
# initialize with 'objectInit' and not 'init', they are queried with
# 'objectProvides' and not 'provides'. On the other hand, service
# instances are services that are NOT intended to be kept in either of
# the controller's lists, they are created by the controller, and they
# are never asked for as normal services.
#
# It is a serious (and as yet undetected) error for a service to be
# created from both 'getService' and 'getServiceInstance'.


sub dispatchMethod {
    my $self = shift;
    my($service, $prefix, $method, @arguments) = @_;
    # the \u makes the first letter of the $command uppercase
    $self->dump(10, "dispatching method '$prefix\u$method'...");
    return ($self->getSelectingServiceList($service)->dispatch($self, "$prefix\u$method", @arguments) or 
            $self->getSelectingObjectList($service)->dispatch($self, "$prefix\u$method", @arguments));
}

sub DESTROY {
    my $self = shift;
    $self->dump(10, 'At controller shutdown, there were ' .
                # I assume there will always be > 1 and so haven't bothered to special case the singular grammar
                scalar(@{$self->services}) . 
                ' services registered, of which ' .
                scalar(keys(%{$self->servicesHash})) .
                ' had been placed in the services hash.')
}


# Implementation Specific Methods
# These should be overriden by real programs

sub registerServices {} # stub
