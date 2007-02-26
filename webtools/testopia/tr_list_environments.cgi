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
# The Original Code is the Bugzilla Testopia System.
#
# The Initial Developer of the Original Code is Greg Hendricks.
# Portions created by Greg Hendricks are Copyright (C) 2006
# Novell. All Rights Reserved.
#
# Contributor(s): Greg Hendricks <ghendricks@novell.com>

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::TestRun;
use Bugzilla::Testopia::Environment;
use Bugzilla::Testopia::Environment::Element;
use Bugzilla::Testopia::Environment::Category;
use Bugzilla::Testopia::Environment::Property;

Bugzilla->login(LOGIN_REQUIRED);


use vars qw($vars);
my $template = Bugzilla->template;
my $cgi = Bugzilla->cgi;

print $cgi->header;

my $action = $cgi->param('action') || '';

$vars->{'qname'} = $cgi->param('qname') if $cgi->param('qname');

$cgi->param('current_tab', 'environment');
my $search = Bugzilla::Testopia::Search->new($cgi);
my $table = Bugzilla::Testopia::Table->new('environment', 'tr_list_environments.cgi', $cgi, undef, $search->query);

$vars->{'table'} = $table;

$template->process("testopia/environment/list.html.tmpl", $vars)
    || print $template->error();
