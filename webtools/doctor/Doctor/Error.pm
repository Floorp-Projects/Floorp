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
# The Original Code is Doctor.
#
# The Initial Developer of the Original Code is Netscape 
# Communications Corporation. Portions created by Netscape 
# are Copyright (C) 2002 Netscape Communications Corporation. 
# All Rights Reserved.
#
# Contributor(s): Myk Melez <myk@mozilla.org>
#                 Frédéric Buclin <LpSolit@gmail.com>

package Doctor::Error;

use strict;

use Doctor qw(%CONFIG);

use base qw(Exporter);
@Doctor::Error::EXPORT = qw(ThrowCodeError ThrowUserError);

sub ThrowUserError {
    # Throw an error about a problem with the user's request.  This function
    # should avoid mentioning system problems displaying the error message, since
    # the user isn't going to care about them and probably doesn't need to deal
    # with them after fixing their own mistake.  Errors should be gentle on 
    # the user, since many "user" errors are caused by bad UI that trip them up.

    # !!! Mail code errors to the system administrator!
    my $cgi = Doctor->cgi;
    my $template = Doctor->template;
    my $vars = {};

    ($vars->{'message'},
     $vars->{'title'},
     $vars->{'cvs_command'},
     $vars->{'cvs_error_code'},
     $vars->{'cvs_error_message'}) = @_;

    print $cgi->header;
    $template->process("user-error.tmpl", $vars)
      || print( ($vars->{'title'} ? "<h1>$vars->{'title'}</h1>" : "") . 
                "<p>$vars->{'message'}</p><p>Please go back and try again.</p>" );
    exit;
}

sub ThrowCodeError {
    # Throw error about a problem with the code.  This function should be
    # apologetic and deferent to the user, since it isn't the user's fault
    # the code didn't work.

    # !!! Mail code errors to the system administrator!
    my $cgi = Doctor->cgi;
    my $template = Doctor->template;
    my $vars = {};

    ($vars->{'message'}, $vars->{'title'}) = @_;
    $vars->{'config'} = \%CONFIG;

    print $cgi->header;
    $template->process("code-error.tmpl", $vars)
      || print("
            <p>
            Unfortunately Doctor has experienced an internal error from which
            it was unable to recover.  More information about the error is
            provided below. Please forward this information along with any
            other information that would help diagnose and fix this problem
            to the system administrator at
            <a href=\"mailto:$CONFIG{ADMIN_EMAIL}\">$CONFIG{ADMIN_EMAIL}</a>.
            </p>
            <p>
            couldn't process error.tmpl template: " . $template->error() . 
            "; error occurred while trying to display error message: " . 
            ($vars->{'title'} ? "$vars->{'title'}: ": "") . $vars->{'message'} .
            "</p>");
    exit;
}

1;
