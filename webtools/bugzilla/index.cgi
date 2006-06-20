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
# Contributor(s): Jacob Steenhagen <jake@bugzilla.org>
#                 Frédéric Buclin <LpSolit@gmail.com>

###############################################################################
# Script Initialization
###############################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;

# Include the Bugzilla CGI and general utility library.
use lib ".";
require "globals.pl";

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Update;

# Check whether or not the user is logged in
my $user = Bugzilla->login(LOGIN_OPTIONAL);

###############################################################################
# Main Body Execution
###############################################################################

my $cgi = Bugzilla->cgi;
# Force to use HTTPS unless Param('ssl') equals 'never'.
# This is required because the user may want to log in from here.
if (Param('sslbase') ne '' and Param('ssl') ne 'never') {
    $cgi->require_https(Param('sslbase'));
}

my $template = Bugzilla->template;
my $vars = {};

# Return the appropriate HTTP response headers.
print $cgi->header();

if ($user->in_group('admin')) {
    $vars->{'release'} = Bugzilla::Update::get_notifications();
}

# Generate and return the UI (HTML page) from the appropriate template.
$template->process("index.html.tmpl", $vars)
  || ThrowTemplateError($template->error());
