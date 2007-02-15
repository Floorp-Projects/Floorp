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
# The Original Code is the Bugzilla Test Runner System.
#
# The Initial Developer of the Original Code is Maciej Maczynski.
# Portions created by Maciej Maczynski are Copyright (C) 2001
# Maciej Maczynski. All Rights Reserved.
#
# Contributor(s): Greg Hendricks <ghendricks@novell.com>

=head1 NAME

Bugzilla::Testopia::Build - An object representing a Testopia build number

=head1 DESCRIPTION

Builds are used to classify test runs. A build represents the results of 
a period of work.

=head1 SYNOPSIS

 $build = Bugzilla::Testopia::Build->new($build_id);
 $build = Bugzilla::Testopia::Build->new(\%build_hash);

=cut

package Bugzilla::Testopia::Build;

use strict;

use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Testopia::TestPlan;
use Bugzilla::Testopia::TestCase;

use base qw(Exporter);

###############################
####    Initialization     ####
###############################

=head1 FIELDS

    build_id
    product_id
    name
    description
    milestone
    isactive

=cut

use constant DB_COLUMNS => qw(
    build_id
    product_id
    name
    description
    milestone
    isactive
);

our $columns = join(", ", DB_COLUMNS);


###############################
####       Methods         ####
###############################

=head1 METHODS

=head2 new

Instantiates a new Build object

=cut

sub new {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
    my $self = {};
    bless($self, $class);
    return $self->_init(@_);
}

=head2 _init

Private constructor for build class

=cut

sub _init {
    my $self = shift;
    my ($param) = (@_);
    my $dbh = Bugzilla->dbh;

    my $id = $param unless (ref $param eq 'HASH');
    my $obj;

    if (defined $id && detaint_natural($id)) {

        $obj = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM test_builds
            WHERE build_id = ?}, undef, $id);
    } elsif (ref $param eq 'HASH'){
         $obj = $param;   
    }
    return undef unless (defined $obj);

    foreach my $field (keys %$obj) {
        $self->{$field} = $obj->{$field};
    }
    return $self;
}

=head2 store

Serializes this build to the database

=cut

sub store {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    $dbh->do("INSERT INTO test_builds ($columns)
              VALUES (?,?,?,?,?,?)",
              undef, (undef, $self->{'product_id'}, $self->{'name'}, 
              $self->{'description'}, $self->{'milestone'}, $self->{'isactive'}));
    my $key = $dbh->bz_last_key( 'test_builds', 'build_id' );
    return $key;
}

=head2 remove

Removes this build from the specified product

=cut

sub remove {
    my $self = shift;
    my ($plan) = @_;
    ThrowUserError("testopia-incorrect-plan") if ($self->{'product_id'} != $plan->product_id);
    ThrowUserError("testopia-non-zero-case-run-count") if ($self->{'case_run_count'});
    ThrowUserError("testopia-non-zero-run-count", {'object' => 'Build'}) if ($self->{'run_count'});
    my $dbh = Bugzilla->dbh;
    $dbh->do("DELETE FROM test_builds
              WHERE build_id = ?", undef,
              $self->{'build_id'});
}

=head2 check_name

Returns true if a build of the specified name exists in the database
for a product.

=cut

sub check_name {
    my $self = shift;
    my ($name) = @_;
    my $dbh = Bugzilla->dbh;
    my $is = $dbh->selectrow_array(
        "SELECT build_id FROM test_builds 
         WHERE name = ? AND product_id = ?",
         undef, $name, $self->{'product_id'});
 
    return $is;
}

=head2 check_build_by_name

Returns id of a build of the specified name

=cut

sub check_build_by_name {
    my $self = shift;
    my ($name) = @_;
    my $dbh = Bugzilla->dbh;
    my $id = $dbh->selectrow_array(
        "SELECT build_id FROM test_builds 
         WHERE name = ?", undef, $name);
 
    return $id;
}

=head2 update

Updates an existing build object in the database.
Takes the new name, description, and milestone.

=cut

sub update {
    my $self = shift;
    my ($name, $desc, $milestone, $isactive) = @_;
    my $dbh = Bugzilla->dbh;
    $dbh->do("UPDATE test_builds
                 SET name = ?, description = ?, milestone = ?, isactive= ?
               WHERE build_id = ?", undef,
              ($name, $desc, $milestone, $isactive, $self->{'build_id'}));
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

=head2 milestone

Returns the Bugzilla taget milestone associated with this build

=cut

sub id              { return $_[0]->{'build_id'};   }
sub product_id      { return $_[0]->{'product_id'}; }
sub name            { return $_[0]->{'name'};       }
sub description     { return $_[0]->{'description'};}
sub milestone       { return $_[0]->{'milestone'};}
sub isactive        { return $_[0]->{'isactive'};}

=head2 run_count

Returns the number of test runs using this build

=cut

sub run_count {
    my ($self) = @_;
    my $dbh = Bugzilla->dbh;
    return $self->{'run_count'} if exists $self->{'run_count'};

    $self->{'run_count'} = $dbh->selectrow_array(
          "SELECT COUNT(run_id) FROM test_runs 
           WHERE build_id = ?", undef, $self->{'build_id'});
          
    return $self->{'run_count'};
}

=head2 case_run_count

Returns the number of test case runs against this build

=cut

sub case_run_count {
    my ($self,$status_id) = @_;
    my $dbh = Bugzilla->dbh;
    
    my $query = "SELECT COUNT(case_run_id) FROM test_case_runs 
           WHERE build_id = ?";
    $query .= " AND case_run_status_id = ?" if $status_id;
    
    my $count;
    if ($status_id){
        $count = $dbh->selectrow_array($query, undef, ($self->{'build_id'},$status_id));
    }
    else {
        $count = $dbh->selectrow_array($query, undef, $self->{'build_id'});
    }
          
    return $count;
}

=head1 SEE ALSO

TestPlan TestRun TestCaseRun

=head1 AUTHOR

Greg Hendricks <ghendricks@novell.com>

=cut

1;
