# -*- mode: cperl; c-basic-offset: 8; indent-tabs-mode: nil; -*-

=head1 COPYRIGHT

 # ***** BEGIN LICENSE BLOCK *****
 # Version: MPL 1.1
 #
 # The contents of this file are subject to the Mozilla Public License
 # Version 1.1 (the "License"); you may not use this file except in
 # compliance with the License. You may obtain a copy of the License
 # at http://www.mozilla.org/MPL/
 #
 # Software distributed under the License is distributed on an "AS IS"
 # basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 # the License for the specific language governing rights and
 # limitations under the License.
 #
 # The Original Code is Litmus.
 #
 # The Initial Developer of the Original Code is
 # the Mozilla Corporation.
 # Portions created by the Initial Developer are Copyright (C) 2005
 # the Initial Developer. All Rights Reserved.
 #
 # Contributor(s):
 #   Chris Cooper <ccooper@deadsquid.com>
 #   Zach Lipton <zach@zachlipton.com>
 #
 # ***** END LICENSE BLOCK *****

=cut

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
