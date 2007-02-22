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

package Bugzilla::Testopia::Constants;
use strict;
use base qw(Exporter);

@Bugzilla::Testopia::Constants::EXPORT = qw(
PROPOSED
CONFIRMED
DISABLED

IDLE
PASSED
FAILED
RUNNING
PAUSED
BLOCKED

TR_READ
TR_WRITE
TR_DELETE
TR_ADMIN

REL_AUTHOR
REL_EDITOR
REL_TESTER
REL_TEST_CC

TR_RELATIONSHIPS

);

#
# Fields to include when exporting a Test Case.
#
# All _id fields but case_id are converted to a string representation.
#
@Bugzilla::Testopia::Constants::TESTCASE_EXPORT = qw(
case_id
summary
set_up
break_down
action
expected_results
alias
arguments
author_id
blocks
case_status_id
category_id
components
creation_date
default_tester_id
depends_on
isautomated
plans
priority_id
requirement
script
tags
version
);

@Bugzilla::Constants::EXPORT_OK = qw(contenttypes);

# Test Case Status
use constant PROPOSED  => 1;
use constant CONFIRMED => 2;
use constant DISABLED  => 3;

# Test case Run Status
use constant IDLE    => 1;
use constant PASSED  => 2;
use constant FAILED  => 3;
use constant RUNNING => 4;
use constant PAUSED  => 5;
use constant BLOCKED => 6;

# Test Plan Permissions (bit flags)
use constant TR_READ    => 1;
use constant TR_WRITE   => 2;
use constant TR_DELETE  => 4;
use constant TR_ADMIN   => 8;

use constant REL_AUTHOR             => 0;
use constant REL_EDITOR             => 1;
use constant REL_TESTER             => 2;
use constant REL_TEST_CC            => 3;

use constant RELATIONSHIPS => REL_AUTHOR, REL_EDITOR, REL_TESTER, REL_TEST_CC;
                              
# Used for global events like EVT_FLAG_REQUESTED
use constant REL_ANY                => 100;

# There are two sorts of event - positive and negative. Positive events are
# those for which the user says "I want mail if this happens." Negative events
# are those for which the user says "I don't want mail if this happens."
#
# Exactly when each event fires is defined in wants_bug_mail() in User.pm; I'm
# not commenting them here in case the comments and the code get out of sync.
use constant EVT_OTHER              => 0;
use constant EVT_ADDED_REMOVED      => 1;
use constant EVT_COMMENT            => 2;
use constant EVT_ATTACHMENT         => 3;
use constant EVT_ATTACHMENT_DATA    => 4;
use constant EVT_PROJ_MANAGEMENT    => 5;
use constant EVT_OPENED_CLOSED      => 6;
use constant EVT_KEYWORD            => 7;
use constant EVT_CC                 => 8;

use constant POS_EVENTS => EVT_OTHER, EVT_ADDED_REMOVED, EVT_COMMENT, 
                           EVT_ATTACHMENT, EVT_ATTACHMENT_DATA, 
                           EVT_PROJ_MANAGEMENT, EVT_OPENED_CLOSED, EVT_KEYWORD,
                           EVT_CC;

use constant EVT_UNCONFIRMED        => 50;
use constant EVT_CHANGED_BY_ME      => 51;

use constant NEG_EVENTS => EVT_UNCONFIRMED, EVT_CHANGED_BY_ME;

# These are the "global" flags, which aren't tied to a particular relationship.
# and so use REL_ANY.
use constant EVT_FLAG_REQUESTED     => 100; # Flag has been requested of me
use constant EVT_REQUESTED_FLAG     => 101; # I have requested a flag

use constant GLOBAL_EVENTS => EVT_FLAG_REQUESTED, EVT_REQUESTED_FLAG;

#  Number of bugs to return in a buglist when performing
#  a fulltext search.
use constant FULLTEXT_BUGLIST_LIMIT => 200;

# Path to sendmail.exe (Windows only)
use constant SENDMAIL_EXE => '/usr/lib/sendmail.exe';

1;
