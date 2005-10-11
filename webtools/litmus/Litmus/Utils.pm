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

# General utility functions

package Litmus::Utils;

use strict;

use Litmus;
use Litmus::Error;
use CGI;

our @ISA = qw(Exporter);
@Litmus::Utils::EXPORT = qw(
    requireField
);


# requireField - checks that $field contains data (other than ---) and throws
# an invalidInputError if it does not.
sub requireField {
    my ($fieldname, $field) = @_;
    
    unless($field && $field ne "---") {
        my $c = new CGI;
        print $c->header();
        invalidInputError("You must make a valid selection for field ".$fieldname.".");
    }
}

1;
