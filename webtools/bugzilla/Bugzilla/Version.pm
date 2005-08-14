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

sub bug_count {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    if (!defined $self->{'bug_count'}) {
        $self->{'bug_count'} = $dbh->selectrow_array(qq{
            SELECT COUNT(*) FROM bugs
            WHERE product_id = ? AND version = ?}, undef,
            ($self->product_id, $self->name)) || 0;
    }
    return $self->{'bug_count'};
}

###############################
#####     Accessors        ####
###############################

sub name       { return $_[0]->{'value'};      }
sub product_id { return $_[0]->{'product_id'}; }

###############################
#####     Subroutines       ###
###############################

sub get_versions_by_product {
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

    my @versions;
    foreach my $value (@$values) {
        push @versions, new Bugzilla::Version($product_id, $value);
    }
    return @versions;
}

sub check_version {
    my ($product, $version_name) = @_;

    $version_name || ThrowUserError('version_not_specified');
    my $version = new Bugzilla::Version($product->id, $version_name);
    unless ($version) {
        ThrowUserError('version_not_valid',
                       {'product' => $product->name,
                        'version' => $version_name});
    }
    return $version;
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

    my $version = Bugzilla::Version::check_version($product_obj,
                                                   'acme_version');

=head1 DESCRIPTION

Version.pm represents a Product Version object.

=head1 METHODS

=over

=item C<new($product_id, $value)>

 Description: The constructor is used to load an existing version
              by passing a product id and a version value.

 Params:      $product_id - Integer with a product id.
              $value - String with a version value.

 Returns:     A Bugzilla::Version object.

=item C<bug_count()>

 Description: Returns the total of bugs that belong to the version.

 Params:      none.

 Returns:     Integer with the number of bugs.

=back

=head1 SUBROUTINES

=over

=item C<get_versions_by_product($product_id)>

 Description: Returns all product versions that belong
              to the supplied product.

 Params:      $product_id - Integer with a product id.

 Returns:     Bugzilla::Version object list.

=item C<check_version($product, $version_name)>

 Description: Checks if the version name exists for the product name.

 Params:      $product - A Bugzilla::Product object.
              $version_name - String with a version name.

 Returns:     Bugzilla::Version object.

=back

=cut
