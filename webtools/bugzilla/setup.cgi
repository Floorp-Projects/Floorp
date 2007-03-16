#!/usr/bin/perl -wT
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
# The Initial Developer of the Original Code is Everything Solved.
# Portions created by Everything Solved are Copyright (C) 2006
# Everything Solved. All Rights Reserved.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>

use strict;
use lib ".";


# The order of these "use" statements is important--we have to have
# CGI before we have CGI::Carp. Without CGI::Carp, "use 5.008" will just throw
# an Internal Server Error if it fails, instead of showing a useful error
# message. And unless we're certain we're on 5.008, we shouldn't compile any
# Bugzilla code, particularly Bugzilla::Constants, because "use constant"
# might not exist.
#
# We can't use Bugzilla::CGI yet, because we're not sure our
# required perl modules are installed yet. However, CGI comes
# with perl.
use CGI;
use CGI::Carp qw(fatalsToBrowser);
use 5.008;
use Bugzilla::Constants;
require 5.008001 if ON_WINDOWS;
use Bugzilla::Install::Requirements;
use Bugzilla::Install::Util qw(display_version_and_os);

local $| = 1;

my $cgi = new CGI;
$cgi->charset('UTF-8');
print $cgi->header(-type => 'text/plain');

display_version_and_os();
my $module_results = check_requirements(1);
Bugzilla::Install::Requirements::print_module_instructions($module_results, 1);