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
#                 Mads Bondo Dydensborg <mbd@dbc.dk>

package Bugzilla::WebService::Product;

use strict;
use base qw(Bugzilla::WebService);
use Bugzilla::Product;
use Bugzilla::User;

# Get the ids of the products the user can search
sub get_selectable_products {
    return {ids => [map {$_->id} @{Bugzilla->user->get_selectable_products}]}; 
}

# Get the ids of the products the user can enter bugs against
sub get_enterable_products {
    return {ids => [map {$_->id} @{Bugzilla->user->get_enterable_products}]}; 
}

# Get the union of the products the user can search and enter bugs against.
sub get_accessible_products {
    my %union = ();
    map $union{ $_->id } = 1, @{Bugzilla->user->get_selectable_products};
    map $union{ $_->id } = 1, @{Bugzilla->user->get_enterable_products};
    return { ids => [keys %union] };
}

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

__END__

=head1 NAME

Bugzilla::Webservice::User - The Product API

=head1 DESCRIPTION

This part of the Bugzilla API allows you to list the available Products and
get information about them.

=head1 METHODS

See L<Bugzilla::WebService> for a description of what B<STABLE>, B<UNSTABLE>,
and B<EXPERIMENTAL> mean, and for more information about error codes.

=head2 List Products

=over

=item C<get_selectable_products> B<UNSTABLE>

Description: Returns a list of the ids of the products the user can search on.

Params:     none

Returns:    A hash containing one item, C<ids>, that contains an array
            of product ids.

=item C<get_enterable_products> B<UNSTABLE>

Description: Returns a list of the ids of the products the user can enter bugs
             against.

Params:     none

Returns:    A hash containing one item, C<ids>, that contains an array
            of product ids.

=item C<get_accessible_products> B<UNSTABLE>

Description: Returns a list of the ids of the products the user can search or 
             enter bugs against.

Params:     none

Returns:    A hash containing one item, C<ids>, that contains an array
            of product ids.

=back

