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
use Bugzilla::User;

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

    if (defined $id) {
        detaint_natural($id)
          || ThrowCodeError('param_must_be_numeric',
                            {function => 'Bugzilla::Component::_init'});

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

sub bug_count {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    if (!defined $self->{'bug_count'}) {
        $self->{'bug_count'} = $dbh->selectrow_array(q{
            SELECT COUNT(*) FROM bugs
            WHERE component_id = ?}, undef, $self->id) || 0;
    }
    return $self->{'bug_count'};
}

sub bug_ids {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    if (!defined $self->{'bugs_ids'}) {
        $self->{'bugs_ids'} = $dbh->selectcol_arrayref(q{
            SELECT bug_id FROM bugs
            WHERE component_id = ?}, undef, $self->id);
    }
    return $self->{'bugs_ids'};
}

sub default_assignee {
    my $self = shift;

    if (!defined $self->{'default_assignee'}) {
        $self->{'default_assignee'} =
            new Bugzilla::User($self->{'initialowner'});
    }
    return $self->{'default_assignee'};
}

sub default_qa_contact {
    my $self = shift;

    if (!defined $self->{'default_qa_contact'}) {
        $self->{'default_qa_contact'} =
            new Bugzilla::User($self->{'initialqacontact'});
    }
    return $self->{'default_qa_contact'};
}

###############################
####      Accessors        ####
###############################

sub id          { return $_[0]->{'id'};          }
sub name        { return $_[0]->{'name'};        }
sub description { return $_[0]->{'description'}; }
sub product_id  { return $_[0]->{'product_id'};  }

###############################
####      Subroutines      ####
###############################

sub check_component {
    my ($product, $comp_name) = @_;

    $comp_name || ThrowUserError('component_blank_name');

    if (length($comp_name) > 64) {
        ThrowUserError('component_name_too_long',
                       {'name' => $comp_name});
    }

    my $component =
        new Bugzilla::Component({product_id => $product->id,
                                 name       => $comp_name});
    unless ($component) {
        ThrowUserError('component_not_valid',
                       {'product' => $product->name,
                        'name' => $comp_name});
    }
    return $component;
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

    my $bug_count          = $component->bug_count();
    my $bug_ids            = $component->bug_ids();
    my $id                 = $component->id;
    my $name               = $component->name;
    my $description        = $component->description;
    my $product_id         = $component->product_id;
    my $default_assignee   = $component->default_assignee;
    my $default_qa_contact = $component->default_qa_contact;

    my $component  = Bugzilla::Component::check_component($product, 'AcmeComp');

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

=item C<bug_count()>

 Description: Returns the total of bugs that belong to the component.

 Params:      none.

 Returns:     Integer with the number of bugs.

=item C<bugs_ids()>

 Description: Returns all bug IDs that belong to the component.
 
 Params:      none.

 Returns:     A reference to an array of bug IDs.

=item C<default_assignee()>

 Description: Returns a user object that represents the default assignee for
              the component.

 Params:      none.

 Returns:     A Bugzilla::User object.

=item C<default_qa_contact()>

 Description: Returns a user object that represents the default QA contact for
              the component.

 Params:      none.

 Returns:     A Bugzilla::User object.

=back

=head1 SUBROUTINES

=over

=item C<check_component($product, $comp_name)>

 Description: Checks if the component name was passed in and if it is a valid
              component.

 Params:      $product - A Bugzilla::Product object.
              $comp_name - String with a component name.

 Returns:     Bugzilla::Component object.
             
=back

=cut
