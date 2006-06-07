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
   name => 'newchangedmail',
   type => 'l',
   default => 'From: bugzilla-daemon
To: %to%
Subject: [Bug %bugid%] %neworchanged%%summary%
%threadingmarker%
X-Bugzilla-Reason: %reasonsheader%
X-Bugzilla-Watch-Reason: %reasonswatchheader%
X-Bugzilla-Product: %product%
X-Bugzilla-Component: %component%
X-Bugzilla-Keywords: %keywords%
X-Bugzilla-Severity: %severity%
X-Bugzilla-Who: %changer%
X-Bugzilla-Status: %status%
X-Bugzilla-Priority: %priority%
X-Bugzilla-Assigned-To: %assignedto%
X-Bugzilla-Target-Milestone: %targetmilestone%
X-Bugzilla-Changed-Fields: %changedfields%

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
  } );
  return @param_list;
}

1;
