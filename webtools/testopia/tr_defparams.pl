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

# Test Runner params:

sub check_image_converter {
    my ($value, $hash) = @_;
    if ($value == 1){
       eval "require Image::Magick";
       return "Error requiring Image::Magick: '$@'" if $@;
    } 
    return "";
}

push @param_list,
(
  # This is already in Bugzilla 2.22
  {
   name => 'convert_uncompressed_images',
   desc => 'If this option is on, attachments with content type image/bmp ' .
           'will be converted to image/png and compressed before uploading to'.
           'the database to conserve disk space.',
   type => 'b',
   default => 0,
   checker => \&check_image_converter
  },
  
  {
   name    => 'private-cases-log', 
   desc    => "If this option is on, the tester cannot view other testers' cases",
   type    => 'b',
   default => 0,
  },

  {
   name    => 'allow-test-deletion', 
   desc    => "If this option is on, users can delete objects including plans and cases",
   type    => 'b',
   default => 0,
  },

  {
   name    => 'print-tag-in-case-log', 
   desc    => 'If this option is on, the entire tag text is printed in a test case '.
              'log entry. Otherwise, only an href to the tag is put there.',
   type    => 'b',
   default => 0,
  },

  {
   name    => 'new-case-action-template',
   desc    => "This is the text to be put in a newly created test case \"action\" field",
   type    => 'l',
   default => qq{<ol>
  <li></li>
</ol>},
  },

  {
   name    => 'new-case-effect-template',
   desc    => "This is the text to be put in a newly created test case \"effect\" field",
   type    => 'l',
   default => qq{<ol>
  <li></li>
</ol>},
  },

  {
   name    => 'bug-to-test-case-summary',
   desc    => 'This is the default summary text used when generating a test case '.
              'out of a bug. The special symbol %id% is replaced with a '.
              "hyperlink to the bug's page",
   type    => 'l',
   default => 'Check bug %id%',
  },

  {
   name    => 'bug-to-test-case-action',
   desc    => 'This is the default action text used when generating a test case '.
              'out of a bug. The special symbol %id% is replaced with a '.
              "hyperlink to the bug's page. The special symbol \%description\% is ".
              "replaced with the bug's summary",
   type    => 'l',
   default => 'Verify if a bug %id% is fixed'
  },

  {
   name    => 'bug-to-test-case-effect',
   desc    => 'This is the default effect text used when generating a test case '.
              'out of a bug. The special symbol %id% is replaced with a '.
              "hyperlink to the bug's page",
   type    => 'l',
   default => 'Bug %id% is fixed',
  },

  {
   name => 'default-test-case-status',
   desc => 'Default status for newly created test cases.',
   type => 's',
   choices => ['PROPOSED', 'CONFIRMED', 'DISABLED'],
   default => 'PROPOSED'
  },

  {
   name    => 'new-testrun-email-notif',
   desc    => 'E-mail message sent to assigned testers when a new test run is started. '.
              'There are some special symbols replaced at run time:<br/>'.
              '%to%: list of assigned testers email addresses<br/>'.
              '%summary%: test run summary<br/>'.
              '%plan%: plan\'s name<br/>'.
              '%plan_id%: plan\'s id<br/>'.
              '%product%: product\'s name<br/>'.
              '%product_id%: product\'s id',
   type    => 'l',
   default => 'From: bugzilla-daemon'."\n".
              'To: %to%'."\n".
              'Subject: Test run started.'."\n".
              "\n".
              'Test run \'%summary%\' for product \'%product%\' and test plan \'%plan%\' has '.
              'just been started.'
  },

  {
   name    => 'case-failed-email-notif',
   desc    => 'E-mail message sent when a test case log is marked as \'failed\'. '.
              'There are some special symbols replaced at run time:<br/>'.
              '%id%: test case log id<br/>'.
              '%manager%: test run\'s manager<br/>'.
              '%test_run%: test run\'s summary<br/>'.
              '%tester%: tester<br/>'.
              '%component%: component\'s name',
   type    => 'l',
   default => 'From: bugzilla-daemon'."\n".
              'To: %manager%'."\n".
              'Subject: Case log \'%id%\' marked as failed.'."\n".
              "\n".
              'Test case log \'%id%\' in test run \'%test_run%\' was marked as \'failed\' by %tester%.'
  },

  {
   name    => 'tester-completed-email-notif',
   desc    => 'E-mail message sent when a tester has run every assigned test case.',
   type    => 'l',
   default => 'From: bugzilla-daemon'."\n".
              'To: %manager%'."\n".
              'Subject: Test run completed for tester.'."\n".
              "\n".
              'Tester %tester% has completed the test run \'%test_run%\'.'
  },

  {
   name    => 'component-completed-email-notif',
   desc    => 'E-mail message sent when every test case of a component is run.',
   type    => 'l',
   default => 'From: bugzilla-daemon'."\n".
              'To: %manager%'."\n".
              'Subject: Test run completed for component.'."\n".
              "\n".
              'Test run \'%test_run%\' completed for component \'%component%\'.'
  },

  {
   name    => 'test-run-completed-email-notif',
   desc    => 'E-mail message sent when every test case in a test run is completed.',
   type    => 'l',
   default => 'From: bugzilla-daemon'."\n".
              'To: %manager%'."\n".
              'Subject: Test run completed.'."\n".
              "\n".
              'Test run \'%test_run%\' completed.'
  }
);

1;


