# -*- Mode: perl; tab-width: 4; indent-tabs-mode: nil; -*-
#
# This file is MPL/GPL dual-licensed under the following terms:
# 
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and
# limitations under the License.
#
# The Original Code is PLIF 1.0.
# The Initial Developer of the Original Code is Ian Hickson.
#
# Alternatively, the contents of this file may be used under the terms
# of the GNU General Public License Version 2 or later (the "GPL"), in
# which case the provisions of the GPL are applicable instead of those
# above. If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the MPL, indicate your decision by
# deleting the provisions above and replace them with the notice and
# other provisions required by the GPL. If you do not delete the
# provisions above, a recipient may use your version of this file
# under either the MPL or the GPL.

package PLIF::Input::Default;
use strict;
use vars qw(@ISA);
use PLIF::Input::Arguments;
@ISA = qw(PLIF::Input::Arguments);
1;

sub applies {
    return 1;
}

sub defaultOutputProtocol {
    return 'stdout';
}

sub getMetaData {
    my $self = shift;
    my($field) = @_;
    if ($field eq 'host') {
        return 'localhost';
    } elsif ($field eq 'acceptType') {
        return 'text/plain';
    } else {
        return $self->SUPER::getMetaData($field);
    }
}

# Everything else that PLIF::Input::Arguments does by default is great
# for this. Namely, no command, and returning 'undef' for everything.
