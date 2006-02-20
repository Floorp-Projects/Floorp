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
#                 Jacob Steenhagen <jake@bugzilla.org>
#                 J. Paul Reed <preed@sigkill.com>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 Joseph Heenan <joseph@heenan.me.uk>
#                 Erik Stambaugh <erik@dasbistro.com>
#                 Frédéric Buclin <LpSolit@gmail.com>
#

package Bugzilla::Config::MTA;

use strict;

use Bugzilla::Config::Common;

$Bugzilla::Config::MTA::sortkey = "10";

sub get_param_list {
  my $class = shift;
  my @param_list = (
  {
   name => 'mail_delivery_method',
   type => 's',
   choices => $^O =~ /MSWin32/i 
                  ? ['smtp', 'testfile', 'sendmail', 'none']
                  : ['sendmail', 'smtp', 'qmail', 'testfile', 'none'],
   default => 'sendmail',
   checker => \&check_mail_delivery_method
  },

  {
   name => 'sendmailnow',
   type => 'b',
   default => 1
  },

  {
   name => 'smtpserver',
   type => 't',
   default => 'localhost'
  },

  {
   name => 'passwordmail',
   type => 'l',
   default => 'From: bugzilla-daemon
To: %mailaddress%
Subject: Your Bugzilla password.

To use the wonders of Bugzilla, you can use the following:

 E-mail address: %login%
       Password: %password%

 To change your password, go to:
 %urlbase%userprefs.cgi
'
  },

  {
   name => 'newchangedmail',
   type => 'l',
   default => 'From: bugzilla-daemon
To: %to%
Subject: [Bug %bugid%] %neworchanged%%summary%
%threadingmarker%
X-Bugzilla-Reason: %reasonsheader%
X-Bugzilla-Product: %product%
X-Bugzilla-Component: %component%
X-Bugzilla-Keywords: %keywords%
X-Bugzilla-Severity: %severity%

%urlbase%show_bug.cgi?id=%bugid%

%diffs%

--%space%
Configure bugmail: %urlbase%userprefs.cgi?tab=email
%reasonsbody%'
  },

  {
   name => 'whinedays',
   type => 't',
   default => 7,
   checker => \&check_numeric
  },

  {
   name => 'whinemail',
   type => 'l',
   default => 'From: %maintainer%
To: %email%
Subject: Your Bugzilla buglist needs attention.

[This e-mail has been automatically generated.]

You have one or more bugs assigned to you in the Bugzilla 
bugsystem (%urlbase%) that require
attention.

All of these bugs are in the NEW or REOPENED state, and have not
been touched in %whinedays% days or more.  You need to take a look
at them, and decide on an initial action.

Generally, this means one of three things:

(1) You decide this bug is really quick to deal with (like, it\'s INVALID),
    and so you get rid of it immediately.
(2) You decide the bug doesn\'t belong to you, and you reassign it to someone
    else.  (Hint: if you don\'t know who to reassign it to, make sure that
    the Component field seems reasonable, and then use the "Reassign bug to
    default assignee of selected component" option.)
(3) You decide the bug belongs to you, but you can\'t solve it this moment.
    Just use the "Accept bug" command.

To get a list of all NEW/REOPENED bugs, you can use this URL (bookmark
it if you like!):

 %urlbase%buglist.cgi?bug_status=NEW&bug_status=REOPENED&assigned_to=%userid%

Or, you can use the general query page, at
%urlbase%query.cgi

Appended below are the individual URLs to get to all of your NEW bugs that
haven\'t been touched for a week or more.

You will get this message once a day until you\'ve dealt with these bugs!

'
  },

  {
   name => 'voteremovedmail',
   type => 'l',
   default => 'From: bugzilla-daemon
To: %to%
Subject: [Bug %bugid%] Some or all of your votes have been removed.

Some or all of your votes have been removed from bug %bugid%.

%votesoldtext%

%votesnewtext%

Reason: %reason%

%urlbase%show_bug.cgi?id=%bugid%
'
  } );
  return @param_list;
}

1;
