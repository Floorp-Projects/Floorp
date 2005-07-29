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
# The Original Code is Litmus.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Zach Lipton <zach@zachlipton.com>

# General error reporting

package Litmus::Error;

use strict;

use Litmus;

our @ISA = qw(Exporter);
@Litmus::Error::EXPORT = qw(
    invalidInputError
    internalError
    lastDitchError
);

# used to alert the user when an unexpected input value is found
sub invalidInputError($) {
    my $message = shift;
    _doError("Invalid Input - $message");
    exit;
}

sub internalError($) {
    my $message = shift;
    _doError("Litmus has suffered an internal error - $message");
    exit;
}

# an error type that does not use a template error message. Used if we 
# can't even process the error template. 
sub lastDitchError($) {
    my $message = shift;
    print "Error - Litmus has suffered a serious fatal internal error - $message";
    exit;
}

sub _doError($) {
    my $message = shift;
    my $vars = {
        message => $message,
    };
    Litmus->template()->process("error/error.html.tmpl", $vars) || 
         lastDitchError(Litmus->template()->error());
}

1;