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
use vars qw($AUTOLOAD);  # it's a package global
use Carp qw(cluck confess); # stack trace versions of warn and die
my $DEBUG = 4; # level of warnings and dumps to print to STDERR (none go to user)
my $USER = 1; # level of errors to report to user (all go to STDERR)
my @FATAL = (); # a list of pointers to functions that want to report errors to the user
my $LOCKED = 0; # set to '1' while we are calling the error reporting code
1;

# PLIF = Program Logic Insulation Framework

# Levels are assumed to be something along the following:
# Things that should never come up during normal operation:
#  0 = total failure: e.g. no input or output devices
#  1 = fatal errors: e.g. missing databases, broken connections, out of disk space
#  2 = security: e.g. warnings about repeated cracking attempts
#  3 = non-fatal errors: e.g. propagation of eval() errors as warnings
#  4 = important warnings (e.g. unexpected but possibly legitimate lack of data)
#
# Useful debugging information:
#  5 = important events (e.g. application started)
#  6 = debugging remarks for the section currently under test
#  7 = typical checkpoints (e.g. someone tried to do some output)
#  8 = frequently hit typical checkpoints
#  9 = verbose debugging information
# 10 = ridiculously verbose debugging spam
#
# No code in CVS should do anything at level 6, it is reserved for
# personal debugging.

# Note. All of the methods described in this class except for the
# propertyGet, propertySet and propertyExists methods are class
# methods. You can call "$class->notImplemented" without a problem.

# provide a standard virtual constructor
# if already created, merely return $self
sub create {
    my $class = shift;
    if (ref($class)) {
        $class->dump(10, "Tried to call constructor of already existing object $class, so returning same object");
        return $class; # already created, return self
    } else {
        $class->dump(10, "Called constructor of class $class, creating object...");
        my $self = $class->bless(@_); # call our real constructor
        $self->init(@_);
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
    $class->dump(10, "Called service constructor of class $class, creating object...");
    my $self = $class->bless(@_); # call our real constructor
    $self->init(@_);
    return $self;
}

sub init {} # stub for services

# provide a constructor that always constructs a new copy of the
# class. This is used by services that implement factories for objects
# implemented in the same class (e.g., session objects do this).
sub objectCreate {
    my $class = shift;
    if (ref($class)) {
        $class = ref($class);
    }
    $class->dump(10, "Called object constructor of class $class, creating object...");
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
    $name =~ s/^.*://o; # strip fully-qualified portion
    if ($self->propertyImpliedAccessAllowed($name)) {
        if (scalar(@_) == 1) {
            $self->dump(10, "setting implied property '$name' in '$self'");
            return $self->propertySet($name, @_);
        } elsif (scalar(@_) == 0) {
            if ($self->propertyExists($name)) {
                $self->dump(10, "getting implied property '$name' in '$self'");
                return $self->propertyGet($name);
            } else {
                $self->dump(10, "not getting non-existent implied property '$name' in '$self'");
                return $self->propertyGetUndefined($name);
            }
        }
        $self->dump(10, "neither setting nor getting implied property '$name' in '$self'");
    } else {
        $self->dump(10, "not treating '$name' in '$self' as an implied property, regardless of its existence");
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
    $self->dump(10, "checking for existence of property '$name' in '$self'");
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
    $self->error(0, "Internal Error: Tried to access non-existent method '$method' in object '$self'");
}


# DEBUGGING AIDS

sub dump {
    my $self = shift;
    my($level, @data) = @_;
    if ($self->isAtDebugLevel($level)) {
        foreach (@data) {
            print STDERR "$0: ($level) $_\n";
        }
    }
}

sub warn { 
    my $self = shift;
    my($level, @data) = @_;
    if ($self->isAtDebugLevel($level)) {
        $self->dump($level, ('-'x12).' Start of Warning Stack Trace '.('-'x12));
        cluck(@data); # warn with stack trace
        $self->dump($level, ('-'x12).            ('-'x30)            .('-'x12));
    }
}

sub error { 
    my $self = shift;
    my($level, @data) = @_;
    $self->dump(9, "error raised: $data[0]");
    if ($self->isAtUserLevel($level) and not $LOCKED) {
        $LOCKED = 1;
        $self->dump(10, 'calling @FATAL error handlers...');
        foreach my $entry (@FATAL) {
            eval {
                &{$entry->[1]}(@data);
            };
            if ($@) {
                $self->warn(3, 'Error occured during \@FATAL callback of object \''.($entry->[0])."': $@");
            }
        }
        $self->dump(10, 'done calling @FATAL error handlers');
        $LOCKED = 0;
    }
    confess(@data); # die with stack trace
}

# this should not be called with the @data containing a trailing dot
sub assert { 
    my $self = shift;
    my($condition, $level, @data) = @_;
    if (not $condition) {
        $self->error($level, @data);
    }
}

sub debug {
    my $self = shift;
    $self->dump(7, @_);
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

# returns true only if the argument is a debug level that is at least
# as important as the local value of $USER.
sub isAtUserLevel {
    my $self = shift;
    my($level) = @_;
    return ($level <= $USER);
}

# returns a reference to the $DEBUG variable for configuration
# purposes
sub getDebugLevel {
    return \$DEBUG;
}

# returns a reference to the $USER variable for configuration purposes
sub getUserLevel {
    return \$USER;
}

# returns a reference to the @FATAL variable for modules that have
# very exotic needs
sub getFatalHandlerList {
    return \@FATAL;
}

# returns a reference to the $LOCKED variable for modules that which
# to block @FATAL reporting
sub getFatalHandlerLock {
    return \$LOCKED;
}

# if you call this, make sure that you call the next function too,
# guarenteed, otherwise you will never be freed until the app dies.
# of course, if you _are_ the app then I guess it's ok...
sub enableErrorReporting {
    my $self = shift;
    push(@FATAL, [$self, sub { $self->fatalError(@_); }]);
}

sub disableErrorReporting {
    my $self = shift;
    my @OLDFATAL = @FATAL;
    @FATAL = ();
    foreach my $entry (@OLDFATAL) {
        if ($entry->[0] != $self) {
            push(@FATAL, $entry);
        }
    }
}

sub fatalError {} # stub

sub DESTROY {
    my $self = shift;
    $self->dump(10, "Called destructor of object $self...");
}
