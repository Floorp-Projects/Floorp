# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Bradley Baetz <bbaetz@student.usyd.edu.au>
#

package Bugzilla;

use strict;

use Bugzilla::CGI;
use Bugzilla::Template;

sub create {
    my $class = shift;
    my $B = $class->instance;

    # And set up the vars for this request
    $B->_init_transient;

    return $B;
}

# We don't use Class::Singleton, because theres no need. However, I'm keeping
# the same interface in case we do change in the future

my $_instance;
sub instance {
    my $class = shift;

    $_instance = $class->_new_instance unless ($_instance);

    return $_instance;
}

sub template { return $_[0]->{_template}; }
sub cgi { return $_[0]->{_cgi}; }

# PRIVATE methods below here

# Called from instance
sub _new_instance {
    my $class = shift;

    my $self = { };
    bless($self, $class);

    $self->_init_persistent;

    return $self;
}

# Initialise persistent items
sub _init_persistent {
    my $self = shift;

    # Set up the template
    $self->{_template} = Bugzilla::Template->create();
}

# Initialise transient (per-request) items
sub _init_transient {
    my $self = shift;

    $self->{_cgi} = new Bugzilla::CGI if exists $::ENV{'GATEWAY_INTERFACE'};
}

# Clean up transient items such as database handles
sub _cleanup {
    my $self = shift;

    delete $self->{_cgi};
}

sub DESTROY {
    my $self = shift;

    # Clean up transient items. We can't just let perl handle removing
    # stuff from the $self hash because some stuff (eg database handles)
    # may need special casing
    # under a persistent environment (ie mod_perl)
    $self->_cleanup;
}

1;

__END__

=head1 NAME

Bugzilla - Semi-persistent collection of various objects used by scripts
and modules

=head1 SYNOPSIS

  use Bugzilla;

  Bugzilla->create;

  sub someModulesSub {
    my $B = Bugzilla->instance;
    $B->template->process(...);
  }

=head1 DESCRIPTION

Several Bugzilla 'things' are used by a variety of modules and scripts. This
includes database handles, template objects, and so on.

This module is a singleton intended as a central place to store these objects.
This approach has several advantages:

=over 4

=item *

They're not global variables, so we don't have issues with them staying arround
with mod_perl

=item *

Everything is in one central place, so its easy to access, modify, and maintain

=item *

Code in modules can get access to these objects without having to have them
all passed from the caller, and the caller's caller, and....

=item *

We can reuse objects across requests using mod_perl where appropriate (eg
templates), whilst destroying those which are only valid for a single request
(such as the current user)

=back

Note that items accessible via this object may be loaded when the Bugzilla
object is created, or may be demand-loaded when requested.

For something to be added to this object, it should either be able to benefit
from persistence when run under mod_perl (such as the a C<template> object),
or should be something which is globally required by a large ammount of code
(such as the current C<user> object).

=head1 CREATION

=over 4

=item C<create>

Creates the C<Bugzilla> object, and initialises any per-request data

=item C<instance>

Returns the current C<Bugzilla> instance. If one doesn't exist, then it will
be created, but no per-request data will be set. The only use this method has
for creating the object is from a mod_perl init script. (Its also what
L<Class::Singleton> does, and I'm trying to keep that interface for this)

=back

=head1 FUNCTIONS

=over 4

=item C<template>

The current C<Template> object, to be used for output

=item C<cgi>

The current C<cgi> object. Note that modules should B<not> be using this in
general. Not all Bugzilla actions are cgi requests. Its useful as a convenience
method for those scripts/templates which are only use via CGI, though.

=back
