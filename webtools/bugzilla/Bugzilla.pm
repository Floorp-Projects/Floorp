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
#                 Erik Stambaugh <erik@dasbistro.com>
#                 A. Karl Kornel <karl@kornel.name>

package Bugzilla;

use strict;

use Bugzilla::Auth;
use Bugzilla::Auth::Login::WWW;
use Bugzilla::CGI;
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::DB;
use Bugzilla::Template;
use Bugzilla::User;
use Bugzilla::Error;
use Bugzilla::Util;

use File::Basename;

#####################################################################
# Constants
#####################################################################

# Scripts that are not stopped by shutdownhtml being in effect.
use constant SHUTDOWNHTML_EXEMPT => [
    'editparams.cgi',
    'checksetup.pl',
];

# Non-cgi scripts that should silently exit.
use constant SHUTDOWNHTML_EXIT_SILENTLY => [
    'whine.pl'
];

#####################################################################
# Global Code
#####################################################################

# If Bugzilla is shut down, do not allow anything to run, just display a
# message to the user about the downtime and log out.  Scripts listed in 
# SHUTDOWNHTML_EXEMPT are exempt from this message.
#
# This code must go here. It cannot go anywhere in Bugzilla::CGI, because
# it uses Template, and that causes various dependency loops.
if (Param("shutdownhtml") 
    && lsearch(SHUTDOWNHTML_EXEMPT, basename($0)) == -1) 
{
    # Allow non-cgi scripts to exit silently (without displaying any
    # message), if desired. At this point, no DBI call has been made
    # yet, and no error will be returned if the DB is inaccessible.
    if (lsearch(SHUTDOWNHTML_EXIT_SILENTLY, basename($0)) > -1
        && !i_am_cgi())
    {
        exit;
    }

    # For security reasons, log out users when Bugzilla is down.
    # Bugzilla->login() is required to catch the logincookie, if any.
    my $user = Bugzilla->login(LOGIN_OPTIONAL);
    my $userid = $user->id;
    Bugzilla->logout();

    my $template = Bugzilla->template;
    my $vars = {};
    $vars->{'message'} = 'shutdown';
    $vars->{'userid'} = $userid;
    # Generate and return a message about the downtime, appropriately
    # for if we're a command-line script or a CGI sript.
    my $extension;
    if (i_am_cgi() && (!Bugzilla->cgi->param('format') 
                       || Bugzilla->cgi->param('format') eq 'html')) {
        $extension = 'html';
    }
    else {
        $extension = 'txt';
    }
    print Bugzilla->cgi->header() if i_am_cgi();
    my $t_output;
    $template->process("global/message.$extension.tmpl", $vars, \$t_output)
        || ThrowTemplateError($template->error);
    print $t_output . "\n";
    exit;
}

#####################################################################
# Subroutines and Methods
#####################################################################

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

my $_user;
sub user {
    my $class = shift;

    if (not defined $_user) {
        $_user = new Bugzilla::User;
    }

    return $_user;
}

my $_sudoer;
sub sudoer {
    my $class = shift;    
    return $_sudoer;
}

sub sudo_request {
    my $class = shift;
    my $new_user = shift;
    my $new_sudoer = shift;

    $_user = $new_user;
    $_sudoer = $new_sudoer;
    $::userid = $new_user->id;

    # NOTE: If you want to log the start of an sudo session, do it here.

    return;
}

sub login {
    my ($class, $type) = @_;
    my $authenticated_user = Bugzilla::Auth::Login::WWW->login($type);
    
    # At this point, we now know if a real person is logged in.
    # We must now check to see if an sudo session is in progress.
    # For a session to be in progress, the following must be true:
    # 1: There must be a logged in user
    # 2: That user must be in the 'bz_sudoer' group
    # 3: There must be a valid value in the 'sudo' cookie
    # 4: A Bugzilla::User object must exist for the given cookie value
    # 5: That user must NOT be in the 'bz_sudo_protect' group
    my $sudo_cookie = $class->cgi->cookie('sudo');
    detaint_natural($sudo_cookie) if defined($sudo_cookie);
    my $sudo_target;
    $sudo_target = new Bugzilla::User($sudo_cookie) if defined($sudo_cookie);
    if (defined($authenticated_user)                 &&
        $authenticated_user->in_group('bz_sudoers')  &&
        defined($sudo_cookie)                        &&
        defined($sudo_target)                        &&
        !($sudo_target->in_group('bz_sudo_protect'))
       )
    {
        $_user = $sudo_target;
        $_sudoer = $authenticated_user;
        $::userid = $sudo_target->id;

        # NOTE: If you want to do any special logging, do it here.
    }
    else {
        $_user = $authenticated_user;
    }
    
    return $_user;
}

sub logout {
    my ($class, $option) = @_;

    # If we're not logged in, go away
    return unless user->id;

    $option = LOGOUT_CURRENT unless defined $option;
    Bugzilla::Auth::Login::WWW->logout($_user, $option);
}

sub logout_user {
    my ($class, $user) = @_;
    # When we're logging out another user we leave cookies alone, and
    # therefore avoid calling Bugzilla->logout() directly.
    Bugzilla::Auth::Login::WWW->logout($user, LOGOUT_ALL);
}

# just a compatibility front-end to logout_user that gets a user by id
sub logout_user_by_id {
    my ($class, $id) = @_;
    my $user = new Bugzilla::User($id);
    $class->logout_user($user);
}

# hack that invalidates credentials for a single request
sub logout_request {
    undef $_user;
    undef $_sudoer;
    # XXX clean this up eventually
    $::userid = 0;
    # We can't delete from $cgi->cookie, so logincookie data will remain
    # there. Don't rely on it: use Bugzilla->user->login instead!
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

my $_batch;
sub batch {
    my $class = shift;
    my $newval = shift;
    if ($newval) {
        $_batch = $newval;
    }
    return $_batch || 0;
}

sub dbwritesallowed {
    my $class = shift;

    # We can write if we are connected to the main database.
    # Note that if we don't have a shadowdb, then we claim that its ok
    # to write even if we're nominally connected to the shadowdb.
    # This is OK because this method is only used to test if misc
    # updates can be done, rather than anything complicated.
    return $class->dbh == $_dbh_main;
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
    undef $_user;

    # See bug 192531. If we don't clear the possibly active statement handles,
    # then when this is called from the END block, it happens _before_ the
    # destructors in Bugzilla::DB have happened.
    # See http://rt.perl.org/rt2/Ticket/Display.html?id=17450#38810
    # Without disconnecting explicitly here, noone notices, because DBI::END
    # ends up calling DBD::mysql's $drh->disconnect_all, which is a noop.
    # This code is evil, but it needs to be done, at least until SendSQL and
    # friends can be removed
    @Bugzilla::DB::SQLStateStack = ();
    undef $Bugzilla::DB::_current_sth;

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

Note that all C<Bugzilla> functionality is method based; use C<Bugzilla-E<gt>dbh>
rather than C<Bugzilla::dbh>. Nothing cares about this now, but don't rely on
that.

=over 4

=item C<template>

The current C<Template> object, to be used for output

=item C<cgi>

The current C<cgi> object. Note that modules should B<not> be using this in
general. Not all Bugzilla actions are cgi requests. Its useful as a convenience
method for those scripts/templates which are only use via CGI, though.

=item C<user>

C<undef> if there is no currently logged in user or if the login code has not
yet been run.  If an sudo session is in progress, the C<Bugzilla::User>
corresponding to the person who is being impersonated.  If no session is in
progress, the current C<Bugzilla::User>.

=item C<sudoer>

C<undef> if there is no currently logged in user, the currently logged in user
is not in the I<sudoer> group, or there is no session in progress.  If an sudo
session is in progress, returns the C<Bugzilla::User> object corresponding to
the person who logged in and initiated the session.  If no session is in
progress, returns the C<Bugzilla::User> object corresponding to the currently
logged in user.

=item C<sudo_request>
This begins an sudo session for the current request.  It is meant to be 
used when a session has just started.  For normal use, sudo access should 
normally be set at login time.

=item C<login>

Logs in a user, returning a C<Bugzilla::User> object, or C<undef> if there is
no logged in user. See L<Bugzilla::Auth|Bugzilla::Auth>, and
L<Bugzilla::User|Bugzilla::User>.

=item C<logout($option)>

Logs out the current user, which involves invalidating user sessions and
cookies. Three options are available from
L<Bugzilla::Constants|Bugzilla::Constants>: LOGOUT_CURRENT (the
default), LOGOUT_ALL or LOGOUT_KEEP_CURRENT.

=item C<logout_user($user)>

Logs out the specified user (invalidating all his sessions), taking a
Bugzilla::User instance.

=item C<logout_by_id($id)>

Logs out the user with the id specified. This is a compatibility
function to be used in callsites where there is only a userid and no
Bugzilla::User instance.

=item C<logout_request>

Essentially, causes calls to C<Bugzilla-E<gt>user> to return C<undef>. This has the
effect of logging out a user for the current request only; cookies and
database sessions are left intact.

=item C<batch>

Set to true, by calling Bugzilla->batch(1), to indicate that Bugzilla is
being called in a non-interactive manner and errors should be passed to 
die() rather than being sent to a browser and finished with an exit().
Bugzilla->batch will return the current state of this flag.

=item C<dbh>

The current database handle. See L<DBI>.

=item C<dbwritesallowed>

Determines if writes to the database are permitted. This is usually used to
determine if some general cleanup needs to occur (such as clearing the token
table)

=item C<switch_to_shadow_db>

Switch from using the main database to using the shadow database.

=item C<switch_to_main_db>

Change the database object to refer to the main database.

=back
