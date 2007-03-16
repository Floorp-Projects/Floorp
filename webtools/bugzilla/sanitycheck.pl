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
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Frédéric Buclin.
# Portions created by Frédéric Buclin are Copyright (C) 2007
# Frédéric Buclin. All Rights Reserved.
#
# Contributor(s): Frédéric Buclin <LpSolit@gmail.com>

use strict;

use lib qw(.);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::User;
use Bugzilla::Mailer;

use Getopt::Long;

my $verbose = 0; # Return all comments if true, else errors only.
my $login = '';  # Login name of the user which is used to call sanitycheck.cgi.

my $result = GetOptions('verbose' => \$verbose, 'login=s' => \$login);

Bugzilla->usage_mode(USAGE_MODE_CMDLINE);

# Be sure a login name if given.
$login || ThrowUserError('invalid_username');

my $user = new Bugzilla::User({ name => $login })
  || ThrowUserError('invalid_username', { name => $login });

my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;

# Authenticate using this user account.
Bugzilla->set_user($user);

# Pass this param to sanitycheck.cgi.
$cgi->param('verbose', $verbose);

require 'sanitycheck.cgi';

# Now it's time to send an email to the user if there is something to notify.
if ($cgi->param('output')) {
    my $message;
    my $vars = {};
    $vars->{'addressee'} = $user->email;
    $vars->{'output'} = $cgi->param('output');
    $vars->{'error_found'} = $cgi->param('error_found') ? 1 : 0;

    $template->process('email/sanitycheck.txt.tmpl', $vars, \$message)
      || ThrowTemplateError($template->error());

    MessageToMTA($message);
}
