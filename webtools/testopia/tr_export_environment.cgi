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
# Contributor(s): Garrett Braden <garrett@tnbd.org>

# This script expects an env_id in the query string pointing to the
# environment that you want to export to XML and displays the XML in
# a html textarea.


=head1 NAME

tr_export_environment.cgi

=head1 DESCRIPTION

Exports an Environment by env_id from the database to XML and displays the XML in
a html textarea.

=cut

#************************************************** Uses ****************************************************#
use strict;
use CGI;
use lib ".";
use Bugzilla;
use Bugzilla::Util;
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::Environment;
use Bugzilla::Testopia::Environment::Xml;
use vars qw($vars);

#************************************ Variable Declarations/Initialization **********************************# 
Bugzilla->login(LOGIN_REQUIRED);
my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';
my $env_id = $cgi->param('env_id');

#*********************************************   UI Logic    ************************************************#
print $cgi->header;
my $env = Bugzilla::Testopia::Environment->new($env_id);
ThrowUserError("testopia-read-only", {'object' => $env}) unless $env->canview;
my $xml = Bugzilla::Testopia::Environment::Xml->export($env_id);
if (!defined($xml)) {
    $vars->{'tr_error'} .= "Exporting XML Environment Failed.  Please try again.<BR/>";
}
$vars->{'xml'} = $xml;
$template->process("testopia/environment/export.xml.tmpl", $vars) || print $template->error();
