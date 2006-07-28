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
# The Original Code is the Bugzilla Test Runner System.
#
# The Initial Developer of the Original Code is Maciej Maczynski.
# Portions created by Maciej Maczynski are Copyright (C) 2001
# Maciej Maczynski. All Rights Reserved.
#
# Contributor(s): Greg Hendricks <ghendricks@novell.com>

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Config;
use Bugzilla::Testopia::TestCase;
use Bugzilla::Testopia::Util;

Bugzilla->login();

my $cgi = Bugzilla->cgi;

print $cgi->header;
use vars qw($vars $template);
my $template = Bugzilla->template;

push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

my $case = Bugzilla::Testopia::TestCase->new(1);
print join(",", @{$case->get_dep_tree});
