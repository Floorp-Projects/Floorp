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
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dawn Endico <endico@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Joe Robins <jmrobins@tgix.com>
#                 Jake <jake@bugzilla.org>
#                 J. Paul Reed <preed@sigkill.com>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 Christopher Aillon <christopher@aillon.com>
#                 Shane H. W. Travis <travis@sedsystems.ca>

package Bugzilla::Constants;
use strict;
use base qw(Exporter);

@Bugzilla::Constants::EXPORT = qw(
    CONTROLMAPNA
    CONTROLMAPSHOWN
    CONTROLMAPDEFAULT
    CONTROLMAPMANDATORY

    AUTH_OK
    AUTH_NODATA
    AUTH_ERROR
    AUTH_LOGINFAILED
    AUTH_DISABLED

    LOGIN_OPTIONAL
    LOGIN_NORMAL
    LOGIN_REQUIRED

    LOGOUT_ALL
    LOGOUT_CURRENT
    LOGOUT_KEEP_CURRENT

    DEFAULT_FLAG_EMAIL_SETTINGS
    DEFAULT_EMAIL_SETTINGS

    GRANT_DIRECT
    GRANT_DERIVED
    GRANT_REGEXP

    GROUP_MEMBERSHIP
    GROUP_BLESS
    GROUP_VISIBLE

    MAILTO_USER
    MAILTO_GROUP

    DEFAULT_COLUMN_LIST
    DEFAULT_QUERY_NAME

    COMMENT_COLS

    UNLOCK_ABORT
);

@Bugzilla::Constants::EXPORT_OK = qw(contenttypes);

# CONSTANTS
#
# ControlMap constants for group_control_map.
# membercontol:othercontrol => meaning
# Na:Na               => Bugs in this product may not be restricted to this 
#                        group.
# Shown:Na            => Members of the group may restrict bugs 
#                        in this product to this group.
# Shown:Shown         => Members of the group may restrict bugs
#                        in this product to this group.
#                        Anyone who can enter bugs in this product may initially
#                        restrict bugs in this product to this group.
# Shown:Mandatory     => Members of the group may restrict bugs
#                        in this product to this group.
#                        Non-members who can enter bug in this product
#                        will be forced to restrict it.
# Default:Na          => Members of the group may restrict bugs in this
#                        product to this group and do so by default.
# Default:Default     => Members of the group may restrict bugs in this
#                        product to this group and do so by default and
#                        nonmembers have this option on entry.
# Default:Mandatory   => Members of the group may restrict bugs in this
#                        product to this group and do so by default.
#                        Non-members who can enter bug in this product
#                        will be forced to restrict it.
# Mandatory:Mandatory => Bug will be forced into this group regardless.
# All other combinations are illegal.

use constant CONTROLMAPNA => 0;
use constant CONTROLMAPSHOWN => 1;
use constant CONTROLMAPDEFAULT => 2;
use constant CONTROLMAPMANDATORY => 3;

# See Bugzilla::Auth for docs on AUTH_*, LOGIN_* and LOGOUT_*

use constant AUTH_OK => 0;
use constant AUTH_NODATA => 1;
use constant AUTH_ERROR => 2;
use constant AUTH_LOGINFAILED => 3;
use constant AUTH_DISABLED => 4;

use constant LOGIN_OPTIONAL => 0;
use constant LOGIN_NORMAL => 1;
use constant LOGIN_REQUIRED => 2;

use constant LOGOUT_ALL => 0;
use constant LOGOUT_CURRENT => 1;
use constant LOGOUT_KEEP_CURRENT => 2;

use constant contenttypes =>
  {
   "html"=> "text/html" , 
   "rdf" => "application/rdf+xml" , 
   "rss" => "application/rss+xml" ,
   "xml" => "application/xml" , 
   "js"  => "application/x-javascript" , 
   "csv" => "text/plain" ,
   "png" => "image/png" ,
   "ics" => "text/calendar" ,
  };

use constant DEFAULT_FLAG_EMAIL_SETTINGS =>
      "~FlagRequestee~on" .
      "~FlagRequester~on";

# By default, almost all bugmail is turned on, with the exception
# of CC list additions for anyone except the Assignee/Owner.
# If you want to customize the default settings for new users at
# your own site, ensure that each of the lines ends with either
# "~on" or just "~" (for off).

use constant DEFAULT_EMAIL_SETTINGS => 
      "ExcludeSelf~on" .

      "~FlagRequestee~on" .
      "~FlagRequester~on" .

      "~emailOwnerRemoveme~on" .
      "~emailOwnerComments~on" .
      "~emailOwnerAttachments~on" .
      "~emailOwnerStatus~on" .
      "~emailOwnerResolved~on" .
      "~emailOwnerKeywords~on" .
      "~emailOwnerCC~on" .
      "~emailOwnerOther~on" .
      "~emailOwnerUnconfirmed~on" .
  
      "~emailReporterRemoveme~on" .
      "~emailReporterComments~on" .
      "~emailReporterAttachments~on" .
      "~emailReporterStatus~on" .
      "~emailReporterResolved~on" .
      "~emailReporterKeywords~on" .
      "~emailReporterCC~" .
      "~emailReporterOther~on" .
      "~emailReporterUnconfirmed~on" .
  
      "~emailQAcontactRemoveme~on" .
      "~emailQAcontactComments~on" .
      "~emailQAcontactAttachments~on" .
      "~emailQAcontactStatus~on" .
      "~emailQAcontactResolved~on" .
      "~emailQAcontactKeywords~on" .
      "~emailQAcontactCC~" .
      "~emailQAcontactOther~on" .
      "~emailQAcontactUnconfirmed~on" .
  
      "~emailCClistRemoveme~on" .
      "~emailCClistComments~on" .
      "~emailCClistAttachments~on" .
      "~emailCClistStatus~on" .
      "~emailCClistResolved~on" .
      "~emailCClistKeywords~on" .
      "~emailCClistCC~" .
      "~emailCClistOther~on" .
      "~emailCClistUnconfirmed~on" .
  
      "~emailVoterRemoveme~on" .
      "~emailVoterComments~" .
      "~emailVoterAttachments~" .
      "~emailVoterStatus~" .
      "~emailVoterResolved~on" .
      "~emailVoterKeywords~" .
      "~emailVoterCC~" .
      "~emailVoterOther~" .
      "~emailVoterUnconfirmed~";

use constant GRANT_DIRECT => 0;
use constant GRANT_DERIVED => 1;
use constant GRANT_REGEXP => 2;

use constant GROUP_MEMBERSHIP => 0;
use constant GROUP_BLESS => 1;
use constant GROUP_VISIBLE => 2;

use constant MAILTO_USER => 0;
use constant MAILTO_GROUP => 1;

# The default list of columns for buglist.cgi
use constant DEFAULT_COLUMN_LIST => (
    "bug_severity", "priority", "rep_platform","assigned_to",
    "bug_status", "resolution", "short_short_desc"
);

# Used by query.cgi and buglist.cgi as the named-query name
# for the default settings.
use constant DEFAULT_QUERY_NAME => '(Default query)';

# The column length for displayed (and wrapped) bug comments.
use constant COMMENT_COLS => 80;

# used by Bugzilla::DB to indicate that tables are being unlocked
# because of error
use constant UNLOCK_ABORT => 1;

1;
