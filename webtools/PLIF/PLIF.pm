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

package PLIF;
use strict; # require strict adherence to perl standards
use vars qw($AUTOLOAD); # it's a package global
use POSIX qw(strftime); # timestamps in debug output
use PLIF::Exception;
my $DEBUG = 9; # level of warnings and dumps to print to STDERR (none go to user)
1;

# PLIF = Program Logic Insulation Framework

# Levels are assumed to be something along the following:
#  0 = debugging remarks for the section currently under test
#  1 =
#  2 =
#  3 =
#  4 = important warnings (e.g. unexpected but possibly legitimate lack of data)
#  5 = important events (e.g. application started)
#  6 = 
#  7 = typical checkpoints (e.g. someone tried to do some output)
#  8 = frequently hit typical checkpoints
#  9 = verbose debugging information
# 10 = ridiculously verbose debugging spam

# Note. All of the methods described in this class except for the
# propertyGet, propertySet and propertyExists methods are class
# methods. You can call "$class->notImplemented" without a problem.

# provide a standard virtual constructor
# if already created, merely return $self
sub create {
    my $class = shift;
    if (ref($class)) {
        return $class; # already created, return self
    } else {
        my $self = $class->bless(@_); # call our real constructor
        $self->serviceInit(@_);
        return $self;
    }
}

# provide a constructor that always constructs a new copy of the
# class. This is used to create service instances.
sub serviceCreate {
    my $class = shift;
    if (ref($class)) {
        $class = ref($class);
    }
    my $self = $class->bless(@_); # call our real constructor
    $self->serviceInstanceInit(@_);
    return $self;
}

sub init {} # stub for services

sub serviceInit {
    my $self = shift;
    $self->init(@_);
}

sub serviceInstanceInit {
    my $self = shift;
    $self->init(@_);
}

# provide a constructor that always constructs a new copy of the
# class. This is used by services that implement factories for objects
# implemented in the same class (e.g., session objects do this).
sub objectCreate {
    my $class = shift;
    if (ref($class)) {
        $class = ref($class);
    }
    my $self = $class->bless(@_); # call our real constructor
    $self->objectInit(@_);
    return $self;
}

sub objectInit {} # stub for objects

# internals of create and objectCreate
sub bless {
    my $class = shift;
    my $self = {};
    CORE::bless($self, $class);
    return $self;
}

# provide method-like access for any scalars in $self
sub AUTOLOAD {
    my $self = shift;
    my $name = $AUTOLOAD;
    syntaxError "Use of inherited AUTOLOAD for non-method $name is deprecated" if not defined($self);
    $name =~ s/^.*://o; # strip fully-qualified portion
    if ($self->propertyImpliedAccessAllowed($name)) {
        if (scalar(@_) == 1) {
            return $self->propertySet($name, @_);
        } elsif (scalar(@_) == 0) {
            if ($self->propertyExists($name)) {
                return $self->propertyGet($name);
            } else {
                return $self->propertyGetUndefined($name);
            }
        }
    }
    $self->methodMissing($name, @_);
}

sub propertySet {
    # this is not a class method
    my $self = shift;
    my($name, $value) = @_;
    return $self->{$name} = $value;
}

sub propertyExists {
    # this is not a class method
    my $self = shift;
    my($name) = @_;
    $self->assert($name, 0, 'propertyExists() cannot be called without arguments');
    return exists($self->{$name});
}

sub propertyImpliedAccessAllowed {
    # this is not (supposed to be) a class method
    # my $self = shift;
    # my($name) = @_;
    # $self->assert($name, 0, 'propertyImpliedAccessAllowed() cannot be called without arguments');
    return 1;
}

sub propertyGet {
    # this is not a class method
    my $self = shift;
    my($name) = @_;
    return $self->{$name};
}

sub propertyGetUndefined {
    return undef;
}

sub methodMissing {
    my $self = shift;
    my($method) = @_;
    syntaxError "Tried to access non-existent method '$method' in object '$self'";
}


# DEBUGGING AIDS

sub dump {
    my $self = shift;
    my($level, @data) = @_;
    if ($self->isAtDebugLevel($level)) {
        my $time = strftime('%Y-%m-%d %H:%M:%S UTC', gmtime());
        foreach (@data) {
            print STDERR "$0.$$ \@ $time: ($level) $_\n";
        }
    }
}

sub warn { 
    my $self = shift;
    my($level, @data) = @_;
    if ($self->isAtDebugLevel($level)) {
        $self->dump($level, ('-'x20).' Start of Warning Stack Trace '.('-'x20));
        report PLIF::Exception ('message' => join("\n", @data));
        $self->dump($level, ('-'x20).            ('-'x30)            .('-'x20));
    }
}

# raises a generic error with the arguments passed as the message
# use this for internal errors that don't have their own exception objects
# this should not be called with the @data containing a trailing dot
sub error { 
    my $self = shift;
    my($level, @data) = @_;
    # the next three lines are a highly magical incantation to remove
    # this call from the stack so that the stack trace looks like the
    # previous function raised the exception itself
    my $raise = PLIF::Exception->can('raise');
    @_ = ('PLIF::Exception', 'message', join("\n", @data));
    goto &$raise;
}

# this should not be called with the @data containing a trailing dot
sub assert { 
    my $self = shift;
    my($condition, $level, @data) = @_;
    if (not $condition) {
        # the next three lines are a highly magical incantation to remove
        # this call from the stack so that the stack trace looks like the
        # previous function raised the exception itself
        my $raise = PLIF::Exception->can('raise');
        @_ = ('PLIF::Exception', 'message', join("\n", @data));
        goto &$raise;
    }
}

sub notImplemented {
    my $self = shift;
    $self->error(0, 'Internal Error: Method not implemented');
}

# returns true only if the argument is a debug level that is at least
# as important as the local value of $DEBUG.
sub isAtDebugLevel {
    my $self = shift;
    my($level) = @_;
    return ($level <= $DEBUG);
}

# returns a reference to the $DEBUG variable for configuration
# purposes
sub getDebugLevel {
    return \$DEBUG;
}

sub DESTROY {
    my $self = shift;
    $self->dump(10, "Called destructor of object $self...");
}
