#!/usr/bin/perl -wT
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
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Gervase Markham <gerv@gerv.net>
#                 A. Karl Kornel <karl@kornel.name>

use strict;

use lib qw(.);
use Bugzilla;
use Bugzilla::Auth::Login::WWW;
use Bugzilla::CGI;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::User;
use Bugzilla::Util;
use Date::Format;

my $template = Bugzilla->template;
my $cgi = Bugzilla->cgi;

my $action = $cgi->param('action') || 'logout';

my $vars = {};
my $target;

# sudo: Display the sudo information & login page
if ($action eq 'sudo') {
    # We must have a logged-in user to do this
    # That user must be in the 'bz_sudoers' group
    my $user = Bugzilla->login(LOGIN_REQUIRED);
    unless ($user->in_group('bz_sudoers')) {
        ThrowUserError('auth_failure', {  group => 'bz_sudoers',
                                         action => 'begin',
                                         object => 'sudo_session' }
        );
    }
    
    # Do not try to start a new session if one is already in progress!
    if (defined(Bugzilla->sudoer)) {
        ThrowUserError('sudo-in-progress', { target => $user->login });
    }

    # We may have been given a value to put into the field
    # Don't pass it through unless it's actually a user
    # Could the default value be protected?  Maybe, but we will save the
    # disappointment for later!
    if (defined($cgi->param('target_login')) &&
        Bugzilla::User::login_to_id($cgi->param('target_login')) != 0)
    {
        $vars->{'target_login_default'} = $cgi->param('target_login');
    }

    # Show the sudo page
    $vars->{'will_logout'} = 1 if Bugzilla::Auth::Login::WWW->can_logout;
    $target = 'admin/sudo.html.tmpl';
}
# transition-sudo: Validate target, logout user, and redirect for session start
elsif ($action eq 'sudo-transition') {
    # Get the current user
    my $user = Bugzilla->login(LOGIN_REQUIRED);
    unless ($user->in_group('bz_sudoers')) {
        ThrowUserError('auth_failure', {  group => 'bz_sudoers',
                                         action => 'begin',
                                         object => 'sudo_session' }
        );
    }
    
    # Get & verify the target user (the user who we will be impersonating)
    unless (defined($cgi->param('target_login')) &&
            Bugzilla::User::login_to_id($cgi->param('target_login')) != 0)
    {
        ThrowUserError('invalid_username', 
                       { 'name' => $cgi->param('target_login') }
        );
    }
    my $target_user = new Bugzilla::User(
        Bugzilla::User::login_to_id($cgi->param('target_login'))
    );
    unless (defined($target_user) &&
            $target_user->id != 0)
    {
        ThrowUserError('invalid_username', 
                       { 'name' => $cgi->param('target_login') }
        );
    }
    unless (defined($cgi->param('target_login')) &&
            $target_user->id != 0)
    {
        ThrowUserError('invalid_username', 
                       { 'name' => $cgi->param('target_login') }
        );
    }
    if ($target_user->in_group('bz_sudo_protect')) {
        ThrowUserError('sudo_protected', { login => $target_user->login });
    }
    
    # Log out and Redirect user to the new page
    Bugzilla->logout();
    $target = 'relogin.cgi';
    print $cgi->redirect($target . '?action=begin-sudo&target_login=' .
                         url_quote($target_user->login));
    exit;
}
# begin-sudo: Confirm login and start sudo session
elsif ($action eq 'begin-sudo') {
    # We must have a logged-in user to do this
    # That user must be in the 'bz_sudoers' group
    my $user = Bugzilla->login(LOGIN_REQUIRED);
    unless ($user->in_group('bz_sudoers')) {
        ThrowUserError('auth_failure', {  group => 'bz_sudoers',
                                         action => 'begin',
                                         object => 'sudo_session' }
        );
    }
    
    # Get & verify the target user (the user who we will be impersonating)
    unless (defined($cgi->param('target_login')) &&
            Bugzilla::User::login_to_id($cgi->param('target_login')) != 0)
    {
        ThrowUserError('invalid_username', 
                       { 'name' => $cgi->param('target_login') }
        );
    }
    my $target_user = new Bugzilla::User(
        Bugzilla::User::login_to_id($cgi->param('target_login'))
    );
    unless (defined($target_user) &&
            $target_user->id != 0)
    {
        ThrowUserError('invalid_username', 
                       { 'name' => $cgi->param('target_login') }
        );
    }
    unless (defined($cgi->param('target_login')) &&
            $target_user->id != 0)
    {
        ThrowUserError('invalid_username', 
                       { 'name' => $cgi->param('target_login') }
        );
    }
    if ($target_user->in_group('bz_sudo_protect')) {
        ThrowUserError('sudo_protected', { login => $target_user->login });
    }
    
    # Calculate the session expiry time (T + 6 hours)
    my $time_string = time2str('%a, %d-%b-%Y %T %Z', time+(6*60*60), 'GMT');

    # For future sessions, store the unique ID of the target user
    Bugzilla->cgi->send_cookie('-name'    => 'sudo',
                               '-expires' => $time_string,
                               '-value'   => $target_user->id
    );
    
    # For the present, change the values of Bugzilla::user & Bugzilla::sudoer
    Bugzilla->sudo_request($target_user, Bugzilla->user);
    
    # NOTE: If you want to log the start of an sudo session, do it here.
    
    $vars->{'message'} = 'sudo_started';
    $vars->{'target'} = $target_user->login;
    $target = 'global/message.html.tmpl';
}
# end-sudo: End the current sudo session (if one is in progress)
elsif ($action eq 'end-sudo') {
    # Regardless of our state, delete the sudo cookie if it exists
    $cgi->remove_cookie('sudo');

    # Are we in an sudo session?
    Bugzilla->login(LOGIN_OPTIONAL);
    my $sudoer = Bugzilla->sudoer;
    if (defined($sudoer)) {
        Bugzilla->logout_request();
        Bugzilla->sudo_request($sudoer, undef);
    }

    # NOTE: If you want to log the end of an sudo session, so it here.
    
    $vars->{'message'} = 'sudo_ended';
    $target = 'global/message.html.tmpl';
}
# Log out the currently logged-in user (this used to be the only thing this did)
elsif ($action eq 'logout') {
    # We don't want to remove a random logincookie from the db, so
    # call Bugzilla->login(). If we're logged in after this, then
    # the logincookie must be correct
    Bugzilla->login(LOGIN_OPTIONAL);

    $cgi->remove_cookie('sudo');

    Bugzilla->logout();

    my $template = Bugzilla->template;
    my $cgi = Bugzilla->cgi;
    print $cgi->header();

    $vars->{'message'} = "logged_out";
    $target = 'global/message.html.tmpl';
}

# Display the template
print $cgi->header();
$template->process($target, $vars)
      || ThrowTemplateError($template->error());
