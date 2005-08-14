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

package Bugzilla::Product;

use Bugzilla::Component;
use Bugzilla::Classification;
use Bugzilla::Version;
use Bugzilla::Milestone;

use Bugzilla::Util;
use Bugzilla::Group;
use Bugzilla::Error;

use constant DEFAULT_CLASSIFICATION_ID => 1;

###############################
####    Initialization     ####
###############################

use constant DB_COLUMNS => qw(
   products.id
   products.name
   products.classification_id
   products.description
   products.milestoneurl
   products.disallownew
   products.votesperuser
   products.maxvotesperbug
   products.votestoconfirm
   products.defaultmilestone
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
    my ($param) = @_;
    my $dbh = Bugzilla->dbh;

    my $id = $param unless (ref $param eq 'HASH');
    my $product;

    if (defined $id && detaint_natural($id)) {

        $product = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM products
            WHERE id = ?}, undef, $id);

    } elsif (defined $param->{'name'}) {

        trick_taint($param->{'name'});
        $product = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM products
            WHERE name = ?}, undef, $param->{'name'});
    } else {
        ThrowCodeError('bad_arg',
            {argument => 'param',
             function => 'Bugzilla::Product::_init'});
    }

    return undef unless (defined $product);

    foreach my $field (keys %$product) {
        $self->{$field} = $product->{$field};
    }
    return $self;
}

###############################
####       Methods         ####
###############################

sub components {
    my $self = shift;

    if (!defined $self->{components}) {
        $self->{components} =
            Bugzilla::Component::get_components_by_product($self->id);
    }
    return $self->{components}
}

sub classification {
    my $self = shift;

    if (!defined $self->{'classification'}) {
        $self->{'classification'} =
            new Bugzilla::Classification($self->classification_id);
    }
    return $self->{'classification'};
}

sub group_controls {
    my $self = shift;

    if (!defined $self->{group_controls}) {
        $self->{group_controls} =
            Bugzilla::Group::get_group_controls_by_product($self->id);
    }
    return $self->{group_controls};
}

sub versions {
    my $self = shift;

    if (!defined $self->{versions}) {
        $self->{versions} =
            Bugzilla::Version::get_versions_by_product($self->id);

    }
    return $self->{versions};
}

sub milestones {
    my $self = shift;

    if (!defined $self->{milestones}) {
        $self->{milestones} =
            Bugzilla::Milestone::get_milestones_by_product($self->id);
    }
    return $self->{milestones};
}

sub bug_count {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    if (!defined $self->{'bug_count'}) {
        $self->{'bug_count'} = $dbh->selectrow_array(qq{
            SELECT COUNT(bug_id) FROM bugs
            WHERE product_id = ?}, undef, $self->id);

    }
    return $self->{'bug_count'};
}

###############################
####      Accessors      ######
###############################

sub id                { return $_[0]->{'id'};                }
sub name              { return $_[0]->{'name'};              }
sub description       { return $_[0]->{'description'};       }
sub milestone_url     { return $_[0]->{'milestoneurl'};      }
sub disallow_new      { return $_[0]->{'disallownew'};       }
sub votes_per_user    { return $_[0]->{'votesperuser'};      }
sub max_votes_per_bug { return $_[0]->{'maxvotesperbug'};    }
sub votes_to_confirm  { return $_[0]->{'votestoconfirm'};    }
sub default_milestone { return $_[0]->{'defaultmilestone'};  }
sub classification_id { return $_[0]->{'classification_id'}; }

###############################
####      Subroutines    ######
###############################

sub get_products_by_classification {
    my ($class_id) = @_;
    my $dbh = Bugzilla->dbh;
    $class_id ||= DEFAULT_CLASSIFICATION_ID;

    my $stored_class_id = $class_id;
    unless (detaint_natural($class_id)) {
        ThrowCodeError(
            'invalid_numeric_argument',
            {argument => 'product_id',
             value    => $stored_class_id,
             function =>
                'Bugzilla::Product::get_classification_products'}
        );
    }

    my $ids = $dbh->selectcol_arrayref(q{
        SELECT id FROM products
        WHERE classification_id = ? ORDER by name}, undef, $class_id);

    my @products;
    foreach my $id (@$ids) {
        push @products, new Bugzilla::Product($id);
    }
    return @products;
}

sub get_all_products {
    my $dbh = Bugzilla->dbh;

    my $ids = $dbh->selectcol_arrayref(q{
        SELECT id FROM products ORDER BY name});

    my @products;
    foreach my $id (@$ids) {
        push @products, new Bugzilla::Product($id);
    }
    return @products;
}

sub check_product {
    my ($product_name) = @_;

    unless ($product_name) {
        ThrowUserError('product_not_specified');
    }
    my $product = new Bugzilla::Product({name => $product_name});
    unless ($product) {
        ThrowUserError('product_doesnt_exist',
                       {'product' => $product_name});
    }
    return $product;
}

1;

__END__

=head1 NAME

Bugzilla::Product - Bugzilla product class.

=head1 SYNOPSIS

    use Bugzilla::Product;

    my $product = new Bugzilla::Product(1);
    my $product = new Bugzilla::Product('AcmeProduct');

    my $components      = $product->components();
    my $classification  = $product->classification();
    my $hash_ref        = $product->group_controls();
    my $hash_ref        = $product->milestones();
    my $hash_ref        = $product->versions();
    my $bugcount        = $product->bug_count();

    my $id               = $product->id;
    my $name             = $product->name;
    my $description      = $product->description;
    my $milestoneurl     = $product->milestone_url;
    my disallownew       = $product->disallow_new;
    my votesperuser      = $product->votes_per_user;
    my maxvotesperbug    = $product->max_votes_per_bug;
    my votestoconfirm    = $product->votes_to_confirm;
    my $defaultmilestone = $product->default_milestone;
    my $classificationid = $product->classification_id;

    my @products = Bugzilla::Product::get_products_by_classification(1);

=head1 DESCRIPTION

Product.pm represents a product object.

=head1 METHODS

=over

=item C<new($param)>

 Description: The constructor is used to load an existing product
              by passing a product id or a hash.

 Params:      $param - If you pass an integer, the integer is the
                       product id from the database that we want to
                       read in. If you pass in a hash with 'name' key,
                       then the value of the name key is the name of a
                       product from the DB.

 Returns:     A Bugzilla::Product object.

=item C<components()>

 Description: Returns a hash with all product components.

 Params:      none.

 Returns:     A hash where component id is the hash key and
              Bugzilla::Component object is the hash value.

=item C<classification()>

 Description: Returns a Bugzilla::Classification object for
              the product classification.

 Params:      none.

 Returns:     A Bugzilla::Classification object.

=item C<group_controls()>

 Description: Returns a hash (group id as key) with all product
              group controls.

 Params:      none.

 Returns:     A hash with group id as key and hash containing the
              group data as value.

=item C<versions()>

 Description: Returns a hash with of all product versions.

 Params:      none.

 Returns:     A hash with version id as key and a Bugzilla::Version
              as value.

=item C<milestones()>

 Description: Returns a hash with of all product milestones.

 Params:      none.

 Returns:     A hash with milestone id as key and a Bugzilla::Milestone
              as value.

=item C<bug_count()>

 Description: Returns the total of bugs that belong to the product.

 Params:      none.

 Returns:     Integer with the number of bugs.

=back

=head1 SUBROUTINES

=over

=item C<get_products_by_classification($class_id)>

 Description: Returns all products for a specific classification id.

 Params:      $class_id - Integer with classification id.

 Returns:     Bugzilla::Product object list.

=item C<get_all_products()>

 Description: Returns all products from the database.

 Params:      none.

 Returns:     Bugzilla::Product object list.

=item C<check_product($product_name)>

 Description: Checks if the product name was passed in and if is a valid
              product.

 Params:      $product_name - String with a product name.

 Returns:     Bugzilla::Product object.

=back

=cut
