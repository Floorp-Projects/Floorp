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

package PLIF::Output::HTTP;
use strict;
use vars qw(@ISA);
use PLIF::Output;
@ISA = qw(PLIF::Output);
1;

sub protocol {
    return 'http';
}

sub finaliseHeader {
    my $self = shift;
    print "Content-Type: " . $self->format . "\n";
    foreach my $header ($self->headers) {
        print "$header\n";
    }
    print "\n";
}

sub authenticate {
    my $self = shift;
    my $realm = $self->realm;
    print "Status: 401 Unauthorized\nWWW-Authenticate: Basic realm=\"$realm\"\n";
    $self->finaliseHeader();
}

sub header {
    my $self = shift;
    print "Status: 200 OK\n";
    $self->finaliseHeader();
}

sub realm {
    my $self = shift;
    $self->notImplemented();
}

sub headers {
    return ();
}
