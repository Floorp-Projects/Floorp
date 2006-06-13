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

Bugzilla::Testopia::Environment - A test environment

=head1 DESCRIPTION

Environments are a set of paramaters dictating what conditions 
a test was conducted in. Each test run must have an environment. 
Environments can be very simple or very complex. Since there is 
no easy way of defining a limit for an environment, the bulk of 
the information is captured as an XML document. At the very least
however, and environment consists of the OS and platform from 
Bugzilla.

=head1 SYNOPSIS

 $env = Bugzilla::Testopia::Environment->new($env_id);
 $env = Bugzilla::Testopia::Environment->new(\%env_hash);

=cut

package Bugzilla::Testopia::Environment;

use strict;

use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::User;

###############################
####    Initialization     ####
###############################

=head1 FIELDS

    environment_id
    op_sys_id
    rep_platform_id
    name
    xml

=cut

use constant DB_COLUMNS => qw(
    test_environments.environment_id
    test_environments.op_sys_id
    test_environments.rep_platform_id
    test_environments.name
    test_environments.xml
);

our $columns = join(", ", DB_COLUMNS);

###############################
####       Methods         ####
###############################

=head1 METHODS

=head2 new

Instantiates a new Environment object

=cut

sub new {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
    my $self = {};
    bless($self, $class);
    return $self->_init(@_);
}

=head2 _init

Private constructor for the environment class

=cut

sub _init {
    my $self = shift;
    my ($param) = (@_);
    my $dbh = Bugzilla->dbh;

    my $id = $param unless (ref $param eq 'HASH');
    my $obj;

    if (defined $id && detaint_natural($id)) {

        $obj = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM test_environments
            WHERE environment_id = ?}, undef, $id);
    } elsif (ref $param eq 'HASH'){
         $obj = $param;   
    }

    return undef unless (defined $obj);

    foreach my $field (keys %$obj) {
        $self->{$field} = $obj->{$field};
    }
    return $self;
}

=head2 get_op_sys_list

Returns the list of bugzilla op_sys entries

=cut

sub get_op_sys_list{
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my $ref = $dbh->selectall_arrayref(
            "SELECT id, value AS name
             FROM op_sys
             ORDER BY sortkey", {'Slice'=>{}});

    return $ref;             
}

=head2 get_rep_platform_list

Returns the list of rep_platforms from bugzilla

=cut

sub get_rep_platform_list{
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my $ref = $dbh->selectall_arrayref(
            "SELECT id, value AS name
             FROM rep_platform
             ORDER BY sortkey", {'Slice'=>{}});

    return $ref;             
}

=head2 store

Serializes this environment to the database

=cut

sub store {
    my $self = shift;
    my $timestamp = Bugzilla::Testopia::Util::get_time_stamp();
    # Verify name is available
    return 0 if $self->check_name($self->{'name'});

    my $dbh = Bugzilla->dbh;
    $dbh->do("INSERT INTO test_environments ($columns)
              VALUES (?,?,?,?,?)",
              undef, (undef, $self->{'op_sys_id'}, 
              $self->{'rep_platform_id'}, $self->{'name'}, 
              $self->{'xml'}));
    my $key = $dbh->bz_last_key( 'test_plans', 'plan_id' );
    return $key;
}

=head2 update

Updates this environment object in the database.
Takes a reference to a hash whose keys match the fields of 
an environment.

=cut

sub update {
    my $self = shift;
    my ($newvalues) = @_;
    my $dbh = Bugzilla->dbh;

    $dbh->bz_lock_tables('test_environments WRITE');
    foreach my $field (keys %{$newvalues}){
        if ($self->{$field} ne $newvalues->{$field}){
            # If the new name is already in use, return.
            if ($field eq 'name' && $self->check_name($newvalues->{'name'})) {
                $dbh->bz_unlock_tables;
                return 0;
            }
            trick_taint($newvalues->{$field});
            $dbh->do("UPDATE test_environments 
                      SET $field = ? WHERE environment_id = ?",
                      undef, $newvalues->{$field}, $self->{'environment_id'});
            $self->{$field} = $newvalues->{$field};
            
        }
    }
    $dbh->bz_unlock_tables;

    return 1;
}

=head2 obliterate

Completely removes this environment from the database.

=cut

sub obliterate {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    $dbh->bz_lock_tables('test_runs READ', 'test_environments WRITE');
    my $used = $dbh->selectrow_array("SELECT 1 FROM test_runs 
                                      WHERE environment_id = ?",
                                      undef, $self->{'environment_id'});
    if ($used) {
        $dbh->bz_unlock_tables;
        ThrowUserError("testopia-non-zero-run-count", {'object' => 'Environment'});
    }
    $dbh->do("DELETE FROM test_environments 
              WHERE environment_id = ?", undef, $self->{'environment_id'});
    $dbh->bz_unlock_tables;
}

=head2 get_run_list

Returns a list of run ids associated with this environment.

=cut

sub get_run_list {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my $ref = $dbh->selectcol_arrayref("SELECT run_id FROM test_runs 
                                        WHERE environment_id = ?",
                                        undef, $self->{'environment_id'});
    return join(",", @{$ref});
}

=head2 get_run_count

Returns a count of the runs associated with this environment

=cut

sub get_run_count {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my ($count) = $dbh->selectrow_array("SELECT COUNT(run_id) FROM test_runs 
                                        WHERE environment_id = ?",
                                        undef, $self->{'environment_id'});
    return $count;
}

=head2 check_name

Returns true if an environment of the specified name exists 
in the database.

=cut

sub check_name {
    my $self = shift;
    my ($name) = @_;
    my $dbh = Bugzilla->dbh;
    my ($used) = $dbh->selectrow_array("SELECT 1 
                                         FROM test_environments 
                                         WHERE name = ?",
                                         undef, $name);
    return $used;
}

=head2 canedit

Returns true if the logged in user has rights to edit this environment.

=cut

sub canedit {
    my $self = shift;
    return UserInGroup('managetestplans') 
      || UserInGroup('edittestcases')
        || UserInGroup('runtests');
}

=head2 canview

Returns true if the logged in user has rights to view this environment.

=cut

sub canview {
    my $self = shift;
    return UserInGroup('managetestplans') 
      || UserInGroup('edittestcases')
        || UserInGroup('runtests');
}

=head2 candelete

Returns true if the logged in user has rights to delete this environment.

=cut

sub candelete {
    my $self = shift;
    return (UserInGroup('managetestplans') 
        || UserInGroup('edittestcases')) 
            && Param('allow-test-deletion');
}

###############################
####      Accessors        ####
###############################
=head2 id

Returns the ID of this object

=head2 name

Returns the name of this object

=head2 xml

Returnst the xml associated with this environment

=cut

sub id              { return $_[0]->{'environment_id'};  }
sub name            { return $_[0]->{'name'};            }
sub xml             { return $_[0]->{'xml'};             }

=head2 op_sys

Returns the bugzilla op_sys value 

=cut

sub op_sys {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    return $self->{'op_sys'} if exists $self->{'op_sys'};
    my ($res) = $dbh->selectrow_array("SELECT value
                                       FROM op_sys
                                       WHERE id = ?", 
                                       undef, $self->{'op_sys_id'});
    $self->{'op_sys'} = $res;
    return $self->{'op_sys'};

}

=head2 rep_platform

Returns the bugzilla rep_platform value 

=cut

sub rep_platform {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    return $self->{'rep_platform'} if exists $self->{'rep_platform'};
    my ($res) = $dbh->selectrow_array("SELECT value
                                       FROM rep_platform
                                       WHERE id = ?", 
                                       undef, $self->{'rep_platform_id'});
    $self->{'rep_platform'} = $res;
    return $self->{'rep_platform'};
    
}

=head1 TODO

Use Bugzilla methods for op_sys and rep_platforms in 2.22

=head1 SEE ALSO

TestRun

=head1 AUTHOR

Greg Hendricks <ghendricks@novell.com>

=cut

1;
