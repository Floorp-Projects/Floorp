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

package Bugzilla::Version;

use Bugzilla;
use Bugzilla::Util;
use Bugzilla::Error;

################################
#####   Initialization     #####
################################

use constant DEFAULT_VERSION => 'unspecified';

use constant DB_COLUMNS => qw(
    versions.value
    versions.product_id
);

our $columns = join(", ", DB_COLUMNS);

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

    my $version;

    if (defined $product_id
        && detaint_natural($product_id)
        && defined $value) {

        trick_taint($value);
        $version = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM versions
            WHERE value = ?
            AND product_id = ?}, undef, ($value, $product_id));
    } else {
        ThrowCodeError('bad_arg',
            {argument => 'product_id/value',
             function => 'Bugzilla::Version::_init'});
    }

    return undef unless (defined $version);

    foreach my $field (keys %$version) {
        $self->{$field} = $version->{$field};
    }
    return $self;
}

###############################
#####     Accessors        ####
###############################

sub value      { return $_[0]->{'value'};      }
sub product_id { return $_[0]->{'product_id'}; }

###############################
#####     Subroutines       ###
###############################

sub get_versions_by_product ($) {
    my ($product_id) = @_;
    my $dbh = Bugzilla->dbh;

    my $stored_product_id = $product_id;
    unless (detaint_natural($product_id)) {
        ThrowCodeError(
            'invalid_numeric_argument',
            {argument => 'product_id',
             value    => $stored_product_id,
             function =>
                'Bugzilla::Version::get_versions_by_product'}
        );
    }

    my $values = $dbh->selectcol_arrayref(q{
        SELECT value FROM versions
        WHERE product_id = ?}, undef, $product_id);

    my $versions;
    foreach my $value (@$values) {
        $versions->{$value} = new Bugzilla::Version($product_id,
                                                    $value);
    }
    return $versions;
}

1;

__END__

=head1 NAME

Bugzilla::Version - Bugzilla product version class.

=head1 SYNOPSIS

    use Bugzilla::Version;

    my $version = new Bugzilla::Version(1, 'version_value');

    my $product_id = $version->product_id;
    my $value = $version->value;

    my $hash_ref = Bugzilla::Version::get_versions_by_product(1);
    my $version = $hash_ref->{'version_value'};

=head1 DESCRIPTION

Version.pm represents a Product Version object.

=head1 METHODS

=over

=item C<new($product_id, $value)>

 Description: The constructor is used to load an existing version
              by passing a product id and a version value.

 Params:      $product_id - Integer with a Bugzilla product id.
              $value - String with a version value.

 Returns:     A Bugzilla::Version object.

=back

=head1 SUBROUTINES

=over

=item C<get_versions_by_product($product_id)>

 Description: Returns all Bugzilla product versions that belong
              to the supplied product.

 Params:      $product_id - Integer with a Bugzilla product id.

 Returns:     A hash with version value as key and a Bugzilla::Version
              objects as value.

=back

=cut
