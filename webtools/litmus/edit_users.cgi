#!/usr/bin/perl -w
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
# The Original Code is Litmus.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Zach Lipton <zach@zachlipton.com>

use strict;

use Litmus;
use Litmus::Error;
use Litmus::DB::Product;
use Litmus::DB::TestcaseSubgroup;
use Litmus::Auth;
use Litmus::Utils;

use CGI;
use Time::Piece::MySQL;

my $c = Litmus->cgi(); 

# obviously, you need to be an admin to edit users...
Litmus::Auth::requireAdmin('edit_users.cgi');

if ($c->param('search_string')) {
	# search for users:
	my $users = Litmus::DB::User->search_FullTextMatches(
													$c->param('search_string'), 
													$c->param('search_string'));
	my $vars = {
		users => $users,
	};
	print $c->header();
	Litmus->template()->process("admin/edit_users/search_results.html.tmpl", $vars) || 
		internalError(Litmus->template()->error()); 
} elsif ($c->param('id')) {
	# lookup a given user
	my $uid = $c->param('id');
	my $user = Litmus::DB::User->retrieve($uid);
	print $c->header();
	if (! $user) {
		invalidInputError("Invalid user id: $uid");
	}
	my $vars = {
		user => $user,
	};
		Litmus->template()->process("admin/edit_users/edit_user.html.tmpl", $vars) || 
		internalError(Litmus->template()->error()); 
} elsif ($c->param('user_id')) {
	# process changes to a user:
	my $user = Litmus::DB::User->retrieve($c->param('user_id'));
	print $c->header();
	if (! $user) {
		invalidInputError("Invalid user id: " . $c->param('user_id'));
	}
	$user->bugzilla_uid($c->param('bugzilla_uid'));
	$user->email($c->param('email'));

	if ($c->param('password') ne 'unchanged') {
		# they changed the password, so let the auth folks know:
		Litmus::Auth::changePassword($user, $c->param('password'));
	}
	$user->realname($c->param('realname'));
	$user->irc_nickname($c->param('irc_nickname'));
	if ($c->param('enabled')) {
		$user->enabled(1);
	}
	if ($c->param('is_admin')) {
		$user->is_admin(1);
	}
	$user->authtoken($c->param('authtoken'));
	$user->update();
	my $vars = {
		user => $user,
	};
	Litmus->template()->process("admin/edit_users/user_edited.html.tmpl", $vars) || 
		internalError(Litmus->template()->error()); 
} else {
	# we're here for the first time, so display the search form
	my $vars = {
	};
	print $c->header();
	Litmus->template()->process("admin/edit_users/search_users.html.tmpl", $vars) || 
		internalError(Litmus->template()->error()); 
}



