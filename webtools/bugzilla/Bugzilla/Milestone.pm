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
# Contributor(s): Tiago R. Mello <timello@async.com.br>

use strict;

package Bugzilla::Milestone;

use Bugzilla;
use Bugzilla::Util;
use Bugzilla::Error;

################################
#####    Initialization    #####
################################

use constant DEFAULT_SORTKEY => 0;

use constant DB_COLUMNS => qw(
    milestones.value
    milestones.product_id
    milestones.sortkey
);

my $columns = join(", ", DB_COLUMNS);

sub new {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
    my $self = {};
    bless($self, $class);
    return $self->_init(@_);
}

sub _init {
    my $self = shift;
    my ($product_id, $value) = (@_);
    my $dbh = Bugzilla->dbh;

    my $milestone;

    if (defined $product_id
        && detaint_natural($product_id)
        && defined $value) {

        trick_taint($value);
        $milestone = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM milestones
            WHERE value = ?
            AND product_id = ?}, undef, ($value, $product_id));
    } else {
        ThrowCodeError('bad_arg',
            {argument => 'product_id/value',
             function => 'Bugzilla::Milestone::_init'});
    }

    return undef unless (defined $milestone);

    foreach my $field (keys %$milestone) {
        $self->{$field} = $milestone->{$field};
    }
    return $self;
}

################################
#####      Accessors      ######
################################

sub value      { return $_[0]->{'value'};      }
sub product_id { return $_[0]->{'product_id'}; }
sub sortkey    { return $_[0]->{'sortkey'};    }

################################
#####     Subroutines      #####
################################

sub get_milestones_by_product ($) {
    my ($product_id) = @_;
    my $dbh = Bugzilla->dbh;

    my $stored_product_id = $product_id;
    unless (detaint_natural($product_id)) {
        ThrowCodeError(
            'invalid_numeric_argument',
            {argument => 'product_id',
             value    => $stored_product_id,
             function =>
                'Bugzilla::Milestone::get_milestones_by_product'}
        );
    }

    my $values = $dbh->selectcol_arrayref(q{
        SELECT value FROM milestones
        WHERE product_id = ?}, undef, $product_id);

    my $milestones;
    foreach my $value (@$values) {
        $milestones->{$value} = new Bugzilla::Milestone($product_id,
                                                        $value);
    }
    return $milestones;
}

1;

__END__

=head1 NAME

Bugzilla::Milestone - Bugzilla product milestone class.

=head1 SYNOPSIS

    use Bugzilla::Milestone;

    my $milestone = new Bugzilla::Milestone(1, 'milestone_value');

    my $product_id = $milestone->product_id;
    my $value = $milestone->value;

    my $hash_ref = Bugzilla::Milestone::get_milestones_by_product(1);
    my $milestone = $hash_ref->{'milestone_value'};

=head1 DESCRIPTION

Milestone.pm represents a Product Milestone object.

=head1 METHODS

=over

=item C<new($product_id, $value)>

 Description: The constructor is used to load an existing milestone
              by passing a product id and a milestone value.

 Params:      $product_id - Integer with a Bugzilla product id.
              $value - String with a milestone value.

 Returns:     A Bugzilla::Milestone object.

=back

=head1 SUBROUTINES

=over

=item C<get_milestones_by_product($product_id)>

 Description: Returns all Bugzilla product milestones that belong
              to the supplied product.

 Params:      $product_id - Integer with a Bugzilla product id.

 Returns:     A hash with milestone value as key and a
              Bugzilla::Milestone object as hash value.

=back

=cut
