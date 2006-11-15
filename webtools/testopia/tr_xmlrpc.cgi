#!/usr/bin/perl 
####!/usr/bin/perl -d:ptkdb -wT
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
# Contributor(s): Marc Schumann <wurblzap@gmail.com>
#                 Dallas Harken <dharken@novell.com>

sub BEGIN
{
    # For use with ptkdb.
    $ENV{DISPLAY}=":0.0";
}

use strict;
use lib qw(.);

require "globals.pl";

use XMLRPC::Transport::HTTP;
use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::WebService;

# To be used in version 2.23/3.0 of Bugzilla
# Bugzilla->usage_mode(Bugzilla::Constants::USAGE_MODE_WEBSERVICE);

Bugzilla->batch(1);

die 'Content-Type must be "text/xml" when using API' unless
    $ENV{'CONTENT_TYPE'} eq 'text/xml';

my $response = Bugzilla::WebService::XMLRPC::Transport::HTTP::CGI
    ->dispatch_with({'TestPlan'    => 'Bugzilla::WebService::Testopia::TestPlan',
                     'TestCase'    => 'Bugzilla::WebService::Testopia::TestCase',
                     'TestRun'     => 'Bugzilla::WebService::Testopia::TestRun',
                     'TestCaseRun' => 'Bugzilla::WebService::Testopia::TestCaseRun',
                     'User'        => 'Bugzilla::WebService::User',
                     'Product'     => 'Bugzilla::WebService::Testopia::Product',
                     'Build'       => 'Bugzilla::WebService::Testopia::Build',
                    })
    ->handle;
