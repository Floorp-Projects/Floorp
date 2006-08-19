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
# Contributor(s): Marc Schumann <wurblzap@gmail.com>

package Bugzilla::WebService::Product;

use strict;
use base qw(Bugzilla::WebService);
use Bugzilla::Product;

sub get_product {
    my $self = shift;
    my ($product_name) = @_;

    Bugzilla->login;

    # Bugzilla::Product doesn't do permissions checks, so we can't do the call
    # to Bugzilla::Product::new until a permissions check happens here.
    $self->fail_unimplemented();

    return new Bugzilla::Product({'name' => $product_name});
}

1;
