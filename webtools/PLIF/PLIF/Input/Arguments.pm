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

package PLIF::Input::Arguments;
use strict;
use vars qw(@ISA);
use PLIF::Input;
@ISA = qw(PLIF::Input);
1;

sub fetchArguments {
    my $self = shift;
    $self->splitArguments();
    $self->setCommandArgument();
}

# Returns the values given for that argument. In a scalar context,
# returns the first value (or undef if the argument was never
# given). In an array context, returns all the values given.
sub getArgument {
    my $self = shift;
    my($argument) = @_;
    if (not defined($self->{"argument $argument"})) {
        $self->createArgument(@_);
    }
    if (wantarray) {
        return @{$self->{"argument $argument"}};
    } else {
        if (@{$self->{"argument $argument"}}) {
            return $self->{"argument $argument"}->[0];
        } else {
            return undef;
        }
    }
}

# Returns all the arguments present.
sub getArguments {
    my $self = shift;
    my $result = {};
    foreach my $argument (keys(%$self)) {
        if ($argument =~ /^argument (.*)$/o) {
            $result->{$1} = \@{$self->{$argument}};
        }
    }
    return $result;
}

# Returns the values given for that argument if it already exists,
# otherwise undef. In a scalar context, returns the first value (or
# undef if the argument was never given). In an array context, returns
# all the values given. (i.e., the same as getArgument but without the
# implicit call to createArgument)
sub peekArgument {
    my $self = shift;
    my($argument) = @_;
    if (defined($self->{"argument $argument"})) {
        if (wantarray) {
            return @{$self->{"argument $argument"}};
        } elsif (@{$self->{"argument $argument"}}) {
            return $self->{"argument $argument"}->[0];
        }
    }
    return undef;
}


# Specifics of this implementation:

sub splitArguments {} # stub

sub addArgument {
    my $self = shift;
    my($argument, $value) = @_;
    if (not defined($self->{"argument $argument"})) {
        $self->{"argument $argument"} = [];
    }
    push(@{$self->{"argument $argument"}}, $value);
}

sub setArgument {
    my $self = shift;
    my($argument, @value) = @_;
    $self->{"argument $argument"} = \@value;
}

# modifies the last value for this argument to the new value
sub pokeArgument {
    my $self = shift;
    my($argument, $newValue) = @_;
    $self->assert(defined($self->{"argument $argument"}), 1, 'Cannot poke an argument that doesn\'t exist yet');
    $self->assert(@{$self->{"argument $argument"}} > 0, 1, 'Cannot poke an argument that has no value yet');
    $self->{"argument $argument"}->[$#{$self->{"argument $argument"}}] = $newValue;
}

sub resetArguments {
    my $self = shift;
    foreach my $argument (keys(%{$self})) {
        if ($argument =~ /^argument /o) {
            delete($self->{$argument});
        }
    }
}

sub setCommandArgument {
    my $self = shift;
    my $argument = $self->getArgument('');
    if ($argument) {
        $self->command($argument);
    } else {
        $self->command('');
    }
}

sub createArgument {
    my $self = shift;
    my($argument) = @_;
    # drop the default on the floor -- the default should only be used
    # when explicitly requested (e.g. by the user in interactive mode).
    $self->{"argument $argument"} = [];
}

sub propertyExists {
    return 1;
}

sub propertyGet {
    my $self = shift;
    if ($self->SUPER::propertyExists(@_)) {
        return $self->SUPER::propertyGet(@_);
    } else {
        return $self->peekArgument(@_); # XXX assumes that return propagates wantarray context...
        # if not: 
        # my @result = $self->getArgument(@_);
        # if (wantarray) {
        #     return @result;
        # } else {
        #     if (@result) {
        #         return $result[0];
        #     } else {
        #         return undef;
        #     }
        # }
    }
}
