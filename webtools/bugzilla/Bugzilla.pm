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
use Bugzilla::Config;
use Bugzilla::DB;
use Bugzilla::Template;

my $_template;
sub template {
    my $class = shift;
    $_template ||= Bugzilla::Template->create();
    return $_template;
}

my $_cgi;
sub cgi {
    my $class = shift;
    $_cgi ||= new Bugzilla::CGI();
    return $_cgi;
}

my $_dbh;
my $_dbh_main;
my $_dbh_shadow;

sub dbh {
    my $class = shift;

    # If we're not connected, then we must want the main db
    if (!$_dbh) {
        $_dbh = $_dbh_main = Bugzilla::DB::connect_main();
    }

    return $_dbh;
}

sub switch_to_shadow_db {
    my $class = shift;

    if (!$_dbh_shadow) {
        if (Param('shadowdb')) {
            $_dbh_shadow = Bugzilla::DB::connect_shadow();
        } else {
            $_dbh_shadow = $_dbh_main;
        }
    }

    $_dbh = $_dbh_shadow;
}

sub switch_to_main_db {
    my $class = shift;

    $_dbh = $_dbh_main;
}

# Private methods

# Per process cleanup
sub _cleanup {
    undef $_cgi;

    # When we support transactions, need to ->rollback here
    $_dbh_main->disconnect if $_dbh_main;
    $_dbh_shadow->disconnect if $_dbh_shadow and Param("shadowdb");
    undef $_dbh_main;
    undef $_dbh_shadow;
    undef $_dbh;
}

sub END {
    _cleanup();
}

1;

__END__

=head1 NAME

Bugzilla - Semi-persistent collection of various objects used by scripts
and modules

=head1 SYNOPSIS

  use Bugzilla;

  sub someModulesSub {
    Bugzilla->dbh->prepare(...);
    Bugzilla->template->process(...);
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

Note that items accessible via this object are demand-loaded when requested.

For something to be added to this object, it should either be able to benefit
from persistence when run under mod_perl (such as the a C<template> object),
or should be something which is globally required by a large ammount of code
(such as the current C<user> object).

=head1 METHODS

Note that all C<Bugzilla> functionailty is method based; use C<Bugzilla->dbh>
rather than C<Bugzilla::dbh>. Nothing cares about this now, but don't rely on
that.

=over 4

=item C<template>

The current C<Template> object, to be used for output

=item C<cgi>

The current C<cgi> object. Note that modules should B<not> be using this in
general. Not all Bugzilla actions are cgi requests. Its useful as a convenience
method for those scripts/templates which are only use via CGI, though.

=item C<dbh>

The current database handle. See L<DBI>.

=item C<switch_to_shadow_db>

Switch from using the main database to using the shadow database.

=item C<switch_to_main_db>

Change the database object to refer to the main database.

=back
