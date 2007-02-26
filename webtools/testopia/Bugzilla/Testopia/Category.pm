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
# The Original Code is the Bugzilla Testopia System.
#
# The Initial Developer of the Original Code is Greg Hendricks.
# Portions created by Maciej Maczynski are Copyright (C) 2006
# Novell. All Rights Reserved.
#
# Contributor(s): Greg Hendricks <ghendricks@novell.com>

=head1 NAME

Bugzilla::Testopia::Category - An object representing a test case category

=head1 DESCRIPTION

Categories are used to classify test cases. Each test case must
belong to one category.

=head1 SYNOPSIS

 $category = Bugzilla::Testopia::Category->new($category_id);
 $category = Bugzilla::Testopia::Category->new(\%category_hash);

=cut

package Bugzilla::Testopia::Category;

use strict;

use Bugzilla::Util;
use Bugzilla::Testopia::TestPlan;
use Bugzilla::Testopia::TestCase;

###############################
####    Initialization     ####
###############################

=head1 FILEDS

    category_id
    product_id
    name
    description

=cut

use constant DB_COLUMNS => qw(
    category_id
    product_id
    name
    description
);

our $columns = join(", ", DB_COLUMNS);

###############################
####       Methods         ####
###############################

=head1 METHODS

=head2 new

Instantiates a new Category object

=cut

sub new {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
    my $self = {};
    bless($self, $class);
    return $self->_init(@_);
}

=head2 _init

Private constructor for category class

=cut

sub _init {
    my $self = shift;
    my ($param) = (@_);
    my $dbh = Bugzilla->dbh;

    my $id = $param unless (ref $param eq 'HASH');
    my $obj;

    if (defined $id && detaint_natural($id)) {

        $obj = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM test_case_categories
            WHERE category_id = ?}, undef, $id);
    } elsif (ref $param eq 'HASH'){
         $obj = $param;   

    } else {
        ThrowCodeError('bad_arg',
            {argument => 'param',
             function => 'Testopia::Category::_init'});
    }

    return undef unless (defined $obj);

    foreach my $field (keys %$obj) {
        $self->{$field} = $obj->{$field};
    }
    return $self;
}

=head2 store

Serializes this category to the database

=cut

sub store {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    $dbh->do("INSERT INTO test_case_categories ($columns)
              VALUES (?,?,?,?)",
              undef, (undef, $self->{'product_id'}, $self->{'name'}, 
              $self->{'description'}));
    my $key = $dbh->bz_last_key( 'test_case_categories', 'category_id' );
    return $key;
}

=head2 remove

Removes this category from the specified product

=cut

sub remove {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    $dbh->do("DELETE FROM test_case_categories
              WHERE category_id = ?", undef,
              $self->{'category_id'});
}

=head2 check_name

Returns the category id if the specified name exists in the 
database for the product.

=cut

sub check_name {
    my $self = shift;
    my ($name) = @_;
    my $dbh = Bugzilla->dbh;
    my $is = $dbh->selectrow_array(
        "SELECT category_id FROM test_case_categories 
         WHERE name = ? AND product_id = ?",
         undef, $name, $self->{'product_id'});
 
    return $is;
}

=head2 update

Updates an existing category object in the database.
Takes the new name, and description.

=cut

sub update {
    my $self = shift;    
    my ($name, $desc) = @_;
    my $dbh = Bugzilla->dbh;
    $dbh->do("UPDATE test_case_categories
                 SET name = ?, description = ?
               WHERE category_id = ?", undef,
              ($name, $desc, $self->{'category_id'}));
}

sub candelete {
  my $self = shift;
  return 0 unless Bugzilla->user->in_group('Testers');
  return 0 if ($self->case_count);
  return 1;   
}
    
###############################
####      Accessors        ####
###############################

=head2 id

Returns the ID of this object

=head2 product_id

Returns the product_id of this object

=head2 name

Returns the name of this object

=head2 description

Returns the description of this object

=cut

sub id              { return $_[0]->{'category_id'};  }
sub product_id      { return $_[0]->{'product_id'};   }
sub name            { return $_[0]->{'name'};         }
sub description     { return $_[0]->{'description'};  }

=head2 case_count

Returns the number of test cases in this category

=cut

sub case_count {
    my ($self) = @_;
    my $dbh = Bugzilla->dbh;
    return $self->{'case_count'} if exists $self->{'case_count'};

    my ($count) = $dbh->selectrow_array(
          "SELECT COUNT(case_id) 
             FROM test_cases 
            WHERE category_id = ?", 
           undef, $self->{'category_id'});
    $self->{'case_count'} = $count;
    return $self->{'case_count'};
}

=head2 case_ids

Returns a reference to a list of case_ids in this category

=cut

sub case_ids {
    my ($self) = @_;
    my $dbh = Bugzilla->dbh;
    return $self->{'case_ids'} if exists $self->{'case_ids'};

    $self->{'case_ids'} = $dbh->selectcol_arrayref(
          "SELECT case_id 
             FROM test_cases 
            WHERE category_id = ?", 
           undef, $self->{'category_id'});
          
    return $self->{'case_ids'};
}

=head1 SEE ALSO

TestCase

=head1 AUTHOR

Greg Hendricks <ghendricks@novell.com>

=cut

1;
