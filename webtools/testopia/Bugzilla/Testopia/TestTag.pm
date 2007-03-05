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

Bugzilla::Testopia::TestTag - A Testopia tag

=head1 DESCRIPTION

Tags in Testopia are used to classify objects in an intuitive manner.
Tags are user defined strings that can be attached to Test Plans, test
Cases, and Test Runs. They are similar to keywords in Bugzilla but do
not require administrator privileges to create.

=head1 SYNOPSIS

use Bugzilla::Testopia::TestTag;

 $tag = Bugzilla::Testopia::TestTag->new($tag_id);
 $tag = Bugzilla::Testopia::TestTag->new($tag_hash);

=cut

package Bugzilla::Testopia::TestTag;

use strict;

use Bugzilla::Util;
use Bugzilla::Error;

###############################
####    Initialization     ####
###############################

=head1 FIELDS

    tag_id
    tag_name

=cut

use constant DB_COLUMNS => qw(
    tag_id
    tag_name

);

our $columns = join(", ", DB_COLUMNS);


###############################
####       Methods         ####
###############################

=head1 METHODS

=head2 new

Instantiates a new TestTag

=cut

sub new {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
    my $self = {};
    bless($self, $class);
    return $self->_init(@_);
}

=head2 _init

Private constructor

=cut

sub _init {
    my $self = shift;
    my ($param) = (@_);
    my $dbh = Bugzilla->dbh;

    my $id = $param unless (ref $param eq 'HASH');
    my $name = $param unless (ref $param eq 'HASH');
    my $obj;

    if (defined $id && detaint_natural($id)) {

        $obj = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM test_tags
            WHERE tag_id = ?}, undef, $id);
    } elsif ($name){
        $obj = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM test_tags
            WHERE tag_name = ?}, undef, $name);
    } elsif (ref $param eq 'HASH'){
         $obj = $param;   
    } else {
        ThrowCodeError('bad_arg',
            {argument => 'param',
             function => 'Testopia::TestTag::_init'});
    }

    return undef unless (defined $obj);

    foreach my $field (keys %$obj) {
        $self->{$field} = $obj->{$field};
    }
    $self->case_count;
    $self->plan_count;
    $self->run_count;
    return $self;
}

=head2 check_name

Checks the supplied name to see if a tag of that name exists.
If it does it returns a TestTag object. Otherwise it returns
undef.

=cut

sub check_name {
    my $self = shift;
    my ($name) = @_;
    my $dbh = Bugzilla->dbh;
    my ($id) = $dbh->selectrow_array(
            "SELECT tag_id FROM test_tags
              WHERE tag_name = ?",
              undef, lc($name));
    if ($id){
        return Bugzilla::Testopia::TestTag->new($id);
    }
    else{
        return undef;
    }
}

=head2 store

Checks if the given tag exists in the database. If so, it returns
that ID. Otherwise it submits the tag to the database and returns 
the newly created ID.

=cut

sub store {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my $key;
    $self->{'tag_name'} = trim($self->{'tag_name'});
    $dbh->bz_lock_tables('test_tags WRITE');
    ($key) = $dbh->selectrow_array("SELECT tag_id FROM test_tags 
                                    WHERE LOWER(tag_name) = ?", 
                                    undef, lc($self->{'tag_name'}));
    if ($key) {
        $dbh->bz_unlock_tables();
        return $key;
    }
    $dbh->do("INSERT INTO test_tags ($columns)
              VALUES (?,?)",
              undef, (undef, $self->{'tag_name'}));
    $key = $dbh->bz_last_key( 'test_tags', 'tag_id' );
    $dbh->bz_unlock_tables();
    
    $self->{'tag_id'} = $key;
    return $key;
}

=head2 obliterate

Completely removes a tag from the database. This is the only safe
way to do this.

=cut

sub obliterate {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    $dbh->do("DELETE FROM test_case_tags 
              WHERE tag_id = ?", undef, $self->{'tag_id'});
    $dbh->do("DELETE FROM test_plan_tags 
              WHERE tag_id = ?", undef, $self->{'tag_id'});
    $dbh->do("DELETE FROM test_run_tags 
              WHERE tag_id = ?", undef, $self->{'tag_id'});
    $dbh->do("DELETE FROM test_tags 
              WHERE tag_id = ?", undef, $self->{'tag_id'});
}

=head2 case_list

Returns a comma separated list of case ids associated with this tag

=cut

sub case_list {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my $list = $dbh->selectcol_arrayref(
            "SELECT case_id FROM test_case_tags
             WHERE tag_id = ?", undef, $self->{'tag_id'});
    return join(",", @{$list});
}

=head2 plan_list

Returns a comma separated list of plan ids associated with this tag

=cut

sub plan_list {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my $list = $dbh->selectcol_arrayref(
            "SELECT plan_id FROM test_plan_tags
             WHERE tag_id = ?", undef, $self->{'tag_id'});
    return join(",", @{$list});
}

=head2 run_list

Returns a comma separated list of run ids associated with this tag

=cut

sub run_list {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my $list = $dbh->selectcol_arrayref(
            "SELECT run_id FROM test_run_tags
             WHERE tag_id = ?", undef, $self->{'tag_id'});
    return join(",", @{$list});
}

sub candelete {
    my $self = shift;
    return 0 unless Bugzilla->user->in_group("admin");
}

###############################
####      Accessors        ####
###############################

=head1 ACCCESSOR METHODS

=head2 id

Returns the tag ID

=head2 name

Returns the tag name

=cut

sub id      { return $_[0]->{'tag_id'};          }
sub name    { return $_[0]->{'tag_name'};        }

=head2 case_count

Returns a count of the test cases associated with this tag

=cut

sub case_count {
    my $self = shift;
    return $self->{'case_count'} if exists $self->{'case_count'};
    my $dbh = Bugzilla->dbh;
    my ($count) = $dbh->selectrow_array(
            "SELECT COUNT(tag_id) 
             FROM test_case_tags 
             WHERE tag_id = ?", undef, $self->{'tag_id'});
    $self->{'case_count'} = $count;
    return $self->{'case_count'};
}

=head2 plan_count

Returns a count of the test plans associated with this tag

=cut

sub plan_count {
    my $self = shift;
    return $self->{'plan_count'} if exists $self->{'plan_count'};
    my $dbh = Bugzilla->dbh;
    my ($count) = $dbh->selectrow_array(
            "SELECT COUNT(tag_id) 
             FROM test_plan_tags 
             WHERE tag_id = ?", undef, $self->{'tag_id'});

    $self->{'plan_count'} = $count;
    return $self->{'plan_count'};
    
}

=head2 run_count

Returns a count of the test runs associated with this tag

=cut

sub run_count {
    my $self = shift;
    return $self->{'run_count'} if exists $self->{'run_count'};
    my $dbh = Bugzilla->dbh;
    my ($count) = $dbh->selectrow_array(
            "SELECT COUNT(tag_id) 
             FROM test_run_tags 
             WHERE tag_id = ?", undef, $self->{'tag_id'});
    $self->{'run_count'} = $count;
    return $self->{'run_count'}
}

=head1 SEE ALSO

TestPlan TestRun TestCase

=head1 AUTHOR

Greg Hendricks <ghendricks@novell.com>

=cut

1;
