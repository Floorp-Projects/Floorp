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
# Contributor(s): Maciej Maczynski <macmac@xdsnet.pl>
#                 Ed Fuentetaja <efuentetaja@acm.org>
#                 Greg Hendricks <ghendricks@novell.com>

package Bugzilla::Config::Testopia;

use strict;

use Bugzilla::Config::Common;

$Bugzilla::Config::Admin::sortkey = "20";

sub get_param_list {
  my $class = shift;
  my @param_list = (
#  {
#   name    => 'private-cases-log', 
#   type    => 'b',
#   default => 0,
#  },

  {
   name    => 'allow-test-deletion', 
   type    => 'b',
   default => 0,
  },

  {
   name    => 'testopia-allow-group-member-deletes', 
   type    => 'b',
   default => 0,
  },

  {
   name    => 'testopia-default-plan-testers-regexp', 
   type    => 't',
  },

#  {
#   name    => 'print-tag-in-case-log', 
#   type    => 'b',
#   default => 0,
#  },

  {
   name    => 'new-case-action-template',
   type    => 'l',
   default => qq{<ol>
  <li></li>
</ol>},
  },

  {
   name    => 'new-case-results-template',
   type    => 'l',
   default => qq{<ol>
  <li></li>
</ol>},
  },

  {
   name    => 'bug-to-test-case-summary',
   type    => 'l',
   default => 'Test for bug %id% - %summary%',
  },

  {
   name    => 'bug-to-test-case-action',
   type    => 'l',
   default => 'Verify that bug %id% is fixed: %description%'
  },

  {
   name    => 'bug-to-test-case-results',
   type    => 'l',
   default => '',
  },

  {
   name => 'default-test-case-status',
   type => 's',
   choices => ['PROPOSED', 'CONFIRMED', 'DISABLED'],
   default => 'PROPOSED'
  },

#  {
#   name    => 'new-testrun-email-notif',
#   type    => 'l',
#   default => 'From: bugzilla-daemon'."\n".
#              'To: %to%'."\n".
#              'Subject: Test run started.'."\n".
#              "\n".
#              'Test run \'%summary%\' for product \'%product%\' and test plan \'%plan%\' has '.
#              'just been started.'
#  },

#  {
#   name    => 'case-failed-email-notif',
#   type    => 'l',
#   default => 'From: bugzilla-daemon'."\n".
#              'To: %manager%'."\n".
#              'Subject: Case log \'%id%\' marked as failed.'."\n".
#              "\n".
#              'Test case log \'%id%\' in test run \'%test_run%\' was marked as \'failed\' by %tester%.'
#  },

#  {
#   name    => 'tester-completed-email-notif',
#   type    => 'l',
#   default => 'From: bugzilla-daemon'."\n".
#              'To: %manager%'."\n".
#              'Subject: Test run completed for tester.'."\n".
#              "\n".
#              'Tester %tester% has completed the test run \'%test_run%\'.'
#  },

#  {
#   name    => 'test-run-completed-email-notif',
#   type    => 'l',
#   default => 'From: bugzilla-daemon'."\n".
#              'To: %manager%'."\n".
#              'Subject: Test run completed.'."\n".
#              "\n".
#              'Test run \'%test_run%\' completed.'
#  }
);
}
1;
