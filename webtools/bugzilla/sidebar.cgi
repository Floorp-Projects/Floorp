#!/usr/bonsaitools/bin/perl -wT
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
# Contributor(s): Jacob Steenhagen <jake@bugzilla.org>

use strict;

use lib ".";
require "CGI.pl";

# Shut up "Used Only Once" errors
use vars qw(
  $template
  $vars
);

ConnectToDatabase();
quietly_check_login();

###############################################################################
# Main Body Execution
###############################################################################

$vars->{'username'} = $::COOKIE{'Bugzilla_login'} || '';

if (defined $::COOKIE{'Bugzilla_login'}) {
    SendSQL("SELECT mybugslink, userid FROM profiles " .
            "WHERE login_name = " . SqlQuote($::COOKIE{'Bugzilla_login'}));
    my ($mybugslink, $userid) = (FetchSQLData());
    $vars->{'userid'} = $userid;
    $vars->{'canblessanything'} = UserCanBlessAnything();
    if ($mybugslink) {
        my $mybugstemplate = Param("mybugstemplate");
        my %substs = ( 'userid' => url_quote($::COOKIE{'Bugzilla_login'}) );
        $vars->{'mybugsurl'} = PerformSubsts($mybugstemplate, \%substs);
    }
    SendSQL("SELECT name FROM namedqueries WHERE userid = $userid AND linkinfooter");
    while (MoreSQLData()) {
        my ($name) = FetchSQLData();
        push(@{$vars->{'namedqueries'}}, $name);
    }
}

# This sidebar is currently for use with Mozilla based web browsers.
# Internet Explorer 6 is supposed to have a similar feature, but it
# most likely won't support XUL ;)  When that does come out, this
# can be expanded to output normal HTML for IE.  Until then, I like
# the way Scott's sidebar looks so I'm using that as the base for
# this file.
# http://bugzilla.mozilla.org/show_bug.cgi?id=37339

my $useragent = $ENV{HTTP_USER_AGENT};
if ($useragent =~ m:Mozilla/([1-9][0-9]*):i && $1 >= 5 && $useragent !~ m/compatible/i) {
    print "Content-type: application/vnd.mozilla.xul+xml\n\n";
    # Generate and return the XUL from the appropriate template.
    $template->process("sidebar.xul.tmpl", $vars)
      || ThrowTemplateError($template->error());
} else {
    ThrowUserError("sidebar_supports_mozilla_only");
}



