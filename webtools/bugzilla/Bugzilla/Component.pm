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
#

use strict;

package Bugzilla::Component;

use Bugzilla::Util;
use Bugzilla::Error;

###############################
####    Initialization     ####
###############################

use constant DB_COLUMNS => qw(
    components.id
    components.name
    components.product_id
    components.initialowner
    components.initialqacontact
    components.description
);

our $columns = join(", ", DB_COLUMNS);

###############################
####       Methods         ####
###############################

sub new {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
    my $self = {};
    bless($self, $class);
    return $self->_init(@_);
}

sub _init {
    my $self = shift;
    my ($param) = (@_);
    my $dbh = Bugzilla->dbh;

    my $id = $param unless (ref $param eq 'HASH');
    my $component;

    if (defined $id && detaint_natural($id)) {

        $component = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM components
            WHERE id = ?}, undef, $id);

    } elsif (defined $param->{'product_id'}
        && detaint_natural($param->{'product_id'})
        && defined $param->{'name'}) {

        trick_taint($param->{'name'});

        $component = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM components
            WHERE name = ? AND product_id = ?}, undef,
            ($param->{'name'}, $param->{'product_id'}));
    } else {
        ThrowCodeError('bad_arg',
            {argument => 'param',
             function => 'Bugzilla::Component::_init'});
    }

    return undef unless (defined $component);

    foreach my $field (keys %$component) {
        $self->{$field} = $component->{$field};
    }
    return $self;
}

###############################
####      Accessors        ####
###############################

sub id                 { return $_[0]->{'id'};               }
sub name               { return $_[0]->{'name'};             }
sub description        { return $_[0]->{'description'};      }
sub product_id         { return $_[0]->{'product_id'};       }
sub default_assignee   { return $_[0]->{'initialowner'};     }
sub default_qa_contact { return $_[0]->{'initialqacontact'}; }

###############################
####      Subroutines      ####
###############################

sub get_components_by_product {
    my ($product_id) = @_;
    my $dbh = Bugzilla->dbh;

    my $stored_product_id = $product_id;
    unless (detaint_natural($product_id)) {
        ThrowCodeError(
            'invalid_numeric_argument',
            {argument => 'product_id',
             value    => $stored_product_id,
             function =>
                'Bugzilla::Component::get_components_by_product'}
        );
    }

    my $ids = $dbh->selectcol_arrayref(q{
        SELECT id FROM components
        WHERE product_id = ?}, undef, $product_id);

    my $components;
    foreach my $id (@$ids) {
        $components->{$id} = new Bugzilla::Component($id);
    }
    return $components;
}

1;

__END__

=head1 NAME

Bugzilla::Component - Bugzilla product component class.

=head1 SYNOPSIS

    use Bugzilla::Component;

    my $component = new Bugzilla::Component(1);
    my $component = new Bugzilla::Component({product_id => 1,
                                             name       => 'AcmeComp'});

    my $id                 = $component->id;
    my $name               = $component->name;
    my $description        = $component->description;
    my $product_id         = $component->product_id;
    my $default_assignee   = $component->default_assignee;
    my $default_qa_contact = $component->default_qa_contact;

    my $hash_ref = Bugzilla::Component::get_components_by_product(1);
    my $component = $hash_ref->{1};

=head1 DESCRIPTION

Component.pm represents a Product Component object.

=head1 METHODS

=over

=item C<new($param)>

 Description: The constructor is used to load an existing component
              by passing a component id or a hash with the product
              id and the component name.

 Params:      $param - If you pass an integer, the integer is the
                       component id from the database that we want to
                       read in. If you pass in a hash with 'name' key,
                       then the value of the name key is the name of a
                       component from the DB.

 Returns:     A Bugzilla::Component object.

=back

=head1 SUBROUTINES

=over

=item C<get_components_by_product($product_id)>

 Description: Returns all Bugzilla components that belong to the
              supplied product.

 Params:      $product_id - Integer with a Bugzilla product id.

 Returns:     A hash with component id as key and Bugzilla::Component
              object as value.

=back

=cut
