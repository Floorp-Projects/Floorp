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
#                 Dan Mosedale <dmose@mozilla.org>
#                 Joe Robins <jmrobins@tgix.com>
#                 Dave Miller <justdave@syndicomm.com>
#                 Christopher Aillon <christopher@aillon.com>
#                 Gervase Markham <gerv@gerv.net>
#                 Christian Reis <kiko@async.com.br>
#                 Bradley Baetz <bbaetz@acm.org>

package Bugzilla::Auth::CGI;

use strict;

use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Util;

sub login {
    my ($class, $type) = @_;

    # 'NORMAL' logins depend on the 'requirelogin' param
    if ($type == LOGIN_NORMAL) {
        $type = Param('requirelogin') ? LOGIN_REQUIRED : LOGIN_OPTIONAL;
    }

    my $cgi = Bugzilla->cgi;

    # First, try the actual login method against form variables
    my $username = $cgi->param("Bugzilla_login");
    my $passwd = $cgi->param("Bugzilla_password");

    my $authmethod = Param("loginmethod");
    my ($authres, $userid, $extra, $info) =
      Bugzilla::Auth->authenticate($username, $passwd);

    if ($authres == AUTH_OK) {
        # Login via username/password was correct and valid, so create
        # and send out the login cookies
        my $ipaddr = $cgi->remote_addr;
        unless ($cgi->param('Bugzilla_restrictlogin') ||
                Param('loginnetmask') == 32) {
            $ipaddr = Bugzilla::Auth::get_netaddr($ipaddr);
        }

        # The IP address is valid, at least for comparing with itself in a
        # subsequent login
        trick_taint($ipaddr);

        my $dbh = Bugzilla->dbh;
        $dbh->do("INSERT INTO logincookies (userid, ipaddr) VALUES (?, ?)",
                 undef,
                 $userid, $ipaddr);
        my $logincookie = $dbh->selectrow_array("SELECT LAST_INSERT_ID()");

        $cgi->send_cookie(-name => 'Bugzilla_login',
                          -value => $userid,
                          -expires => 'Fri, 01-Jan-2038 00:00:00 GMT');
        $cgi->send_cookie(-name => 'Bugzilla_logincookie',
                          -value => $logincookie,
                          -expires => 'Fri, 01-Jan-2038 00:00:00 GMT');

        # compat code. The cookie value is used for logouts, and that
        # isn't generic yet.
        $::COOKIE{'Bugzilla_logincookie'} = $logincookie;
    } elsif ($authres == AUTH_NODATA) {
        # No data from the form, so try to login via cookies
        $username = $cgi->cookie("Bugzilla_login");
        $passwd = $cgi->cookie("Bugzilla_logincookie");

        require Bugzilla::Auth::Cookie;
        my $authmethod = "Cookie";

        ($authres, $userid, $extra) =
          Bugzilla::Auth::Cookie->authenticate($username, $passwd);

        # If the data for the cookie was incorrect, then treat that as
        # NODATA. This could occur if the user's IP changed, for example.
        # Give them un-loggedin access if allowed (checked below)
        $authres = AUTH_NODATA if $authres == AUTH_LOGINFAILED;
    }

    # Now check the result

    # An error may have occurred with the login mechanism
    if ($authres == AUTH_ERROR) {
        ThrowCodeError("auth_err",
                       { authmethod => lc($authmethod),
                         userid => $userid,
                         auth_err_tag => $extra,
                         info => $info
                       });
    }

    # We can load the page if the login was ok, or there was no data
    # but a login wasn't required
    if ($authres == AUTH_OK ||
        ($authres == AUTH_NODATA && $type == LOGIN_OPTIONAL)) {

        # login succeded, so we're done
        return $userid;
    }

    # No login details were given, but we require a login if the
    # page does
    if ($authres == AUTH_NODATA && $type == LOGIN_REQUIRED) {
        # Throw up the login page

        print Bugzilla->cgi->header();

        my $template = Bugzilla->template;
        $template->process("account/auth/login.html.tmpl",
                           { 'target' => $cgi->url(-relative=>1),
                             'form' => \%::FORM,
                             'mform' => \%::MFORM,
                             'caneditaccount' => Bugzilla::Auth->can_edit,
                           }
                          )
          || ThrowTemplateError($template->error());

        # This seems like as good as time as any to get rid of old
        # crufty junk in the logincookies table.  Get rid of any entry
        # that hasn't been used in a month.
        Bugzilla->dbh->do("DELETE FROM logincookies " .
                          "WHERE TO_DAYS(NOW()) - TO_DAYS(lastused) > 30");

        exit;
    }

    # The username/password may be wrong
    # Don't let the user know whether the username exists or whether
    # the password was just wrong. (This makes it harder for a cracker
    # to find account names by brute force)
    if ($authres == AUTH_LOGINFAILED) {
        ThrowUserError("invalid_username_or_password");
    }

    # The account may be disabled
    if ($authres == AUTH_DISABLED) {
        # Clear the cookie

        $cgi->send_cookie(-name => 'Bugzilla_login',
                          -expires => "Tue, 15-Sep-1998 21:49:00 GMT");
        $cgi->send_cookie(-name => 'Bugzilla_logincookie',
                          -expires => "Tue, 15-Sep-1998 21:49:00 GMT");

        # and throw a user error
        ThrowUserError("account_disabled",
                       {'disabled_reason' => $extra});
    }

    # If we get here, then we've run out of options, which shouldn't happen
    ThrowCodeError("authres_unhandled",
                   { authres => $authres,
                     type => $type,
                   }
                  );

}

1;

__END__

=head1 NAME

Bugzilla::Auth::CGI - CGI-based logins for Bugzilla

=head1 SUMMARY

This is a L<login module|Bugzilla::Auth/"LOGIN"> for Bugzilla. Users connecting
from a CGI script use this module to authenticate.

=head1 BEHAVIOUR

Users are first authenticated against the default authentication handler,
using the CGI parameters I<Bugzilla_login> and I<Bugzilla_password>.

If no data is present for that, then cookies are tried, using
L<Bugzilla::Auth::Cookie>.

=head1 SEE ALSO

L<Bugzilla::Auth>
