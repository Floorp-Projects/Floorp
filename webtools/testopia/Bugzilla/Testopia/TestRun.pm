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
#                 Ed Fuentetaja <efuentetaja@acm.org>

=head1 NAME

Bugzilla::Testopia::TestRun - Testopia Test Run object

=head1 DESCRIPTION

This module represents a test run in Testopia. A test run is the 
place where most of the work of testing is done. A run is associated 
with a single test plan and multiple test cases through the test 
case-runs.

=head1 SYNOPSIS

use Bugzilla::Testopia::TestRun;

 $run = Bugzilla::Testopia::TestRun->new($run_id);
 $run = Bugzilla::Testopia::TestRun->new(\%run_hash);

=cut

package Bugzilla::Testopia::TestRun;

use strict;

use Bugzilla::Util;
use Bugzilla::User;
use Bugzilla::Constants;
use Bugzilla::Config;
use Bugzilla::Testopia::Environment;

use base qw(Exporter);
@Bugzilla::Testopia::TestRun::EXPORT = qw(CalculatePercentCompleted);

###############################
####    Initialization     ####
###############################

=head1 FIELDS

    run_id
    plan_id
    environment_id
    product_version
    build_id
    plan_text_version
    manager_id
    start_date
    stop_date
    summary
    notes

=cut

use constant DB_COLUMNS => qw(
    test_runs.run_id
    test_runs.plan_id
    test_runs.environment_id
    test_runs.product_version
    test_runs.build_id
    test_runs.plan_text_version
    test_runs.manager_id  
    test_runs.start_date
    test_runs.stop_date
    test_runs.summary
    test_runs.notes
);

our $columns = join(", ", DB_COLUMNS);


###############################
####       Methods         ####
###############################

=head2 new

Instantiate a new Test Run. This takes a single argument 
either a test run ID or a reference to a hash containing keys 
identical to a test run's fields and desired values.

=cut

sub new {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
    my $self = {};
    bless($self, $class);
    return $self->_init(@_);
}

=head2 _init

Private constructor for this object 

=cut

sub _init {
    my $self = shift;
    my ($param) = (@_);
    my $dbh = Bugzilla->dbh;

    my $id = $param unless (ref $param eq 'HASH');
    my $obj;

    if (defined $id && detaint_natural($id)) {

        $obj = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM test_runs
            WHERE run_id = ?}, undef, $id);

    } elsif (ref $param eq 'HASH'){
         $obj = $param;   

    } else {
        Bugzilla::Error::ThrowCodeError('bad_arg',
            {argument => 'param',
             function => 'Testopia::TestRun::_init'});
    }

    return undef unless (defined $obj);

    foreach my $field (keys %$obj) {
        $self->{$field} = $obj->{$field};
    }
      
    return $self;
}

=head2 calculate_percent_completed

Calculates a percentage from two numbers. Takes the total number
of IDLE case runs and the number of those that have another status
and adds them to get a total then takes the percentage.

=cut

sub calculate_percent_completed {

  my ($idle, $run) = (@_);
  my $total = $idle + $run;
  my $percent;
  if ($total == 0) {
    $percent = 100;
  } else {
    $percent = $run*100/$total;
    $percent = int($percent + 0.5);
    if (($percent == 100) && ($idle != 0)) {
      #I don't want to see 100% unless every test is run
      $percent = 99;
    }
  }
  return $percent; 
}

=head2 add_cc

Adds a user to the CC list for this run

=cut

sub add_cc{
    my $self = shift;
    my ($ccid) = (@_);
    my $dbh = Bugzilla->dbh;
    $dbh->do("INSERT INTO test_run_cc(run_id, who) 
              VALUES (?,?)", undef, $self->{'run_id'}, $ccid);
    #TODO: send mail
    return 1;
}

=head2 remove_cc

Removes a user from the CC list of this run

=cut

sub remove_cc{
    my $self = shift;
    my ($ccid) = (@_);
    my $dbh = Bugzilla->dbh;
    $dbh->do("DELETE FROM test_run_cc 
              WHERE run_id=? AND who=?", 
              undef, $self->{'run_id'}, $ccid);
    #TODO: send mail
    return 1;
}

=head2 add_tag

Associates a tag with this test case

=cut

sub add_tag {
    my $self = shift;
    my ($tag_id) = @_;
    my $dbh = Bugzilla->dbh;

    $dbh->bz_lock_tables('test_run_tags WRITE');
    my $tagged = $dbh->selectrow_array(
             "SELECT 1 FROM test_run_tags 
              WHERE tag_id = ? AND run_id = ?",
              undef, $tag_id, $self->{'run_id'});
    if ($tagged) {
        $dbh->bz_unlock_tables;
        return 1;
    }
    $dbh->do("INSERT INTO test_run_tags VALUES(?,?)",
              undef, $tag_id, $self->{'run_id'});
    $dbh->bz_unlock_tables;
    
    return 0;
}

=head2 remove_tag

Disassociates a tag from this test case

=cut

sub remove_tag {    
    my $self = shift;
    my ($tag_id) = @_;
    my $dbh = Bugzilla->dbh;
    $dbh->do("DELETE FROM test_run_tags 
              WHERE tag_id=? AND run_id=?",
              undef, $tag_id, $self->{'run_id'});
    return;
}

=head2 add_case_run

Associates a test case with this run by adding a new row to 
the test_case_runs table

=cut

sub add_case_run {
    my $self = shift;
    my ($case_id) = @_;
    my $case = Bugzilla::Testopia::TestCase->new($case_id);
    my $caserun = Bugzilla::Testopia::TestCaseRun->new({
        'run_id' => $self->{'run_id'},
        'case_id' => $case_id,
        'assignee' => $case->default_tester->id,
        'case_text_version' => $case->version,
        'build_id' => $self->build->id,
    });
    $caserun->store;    
}

=head2 store

Stores a test run object in the database. This method is used to store a 
newly created test run. It returns the new ID.

=cut

sub store {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my $timestamp = Bugzilla::Testopia::Util::get_time_stamp();
    $dbh->do("INSERT INTO test_runs ($columns)
              VALUES (?,?,?,?,?,?,?,?,?,?,?)",
              undef, (undef, $self->{'plan_id'}, $self->{'environment_id'}, 
              $self->{'product_version'}, $self->{'build_id'}, 
              $self->{'plan_text_version'}, $self->{'manager_id'}, 
              $timestamp, undef, $self->{'summary'}, $self->{'notes'}));
    my $key = $dbh->bz_last_key( 'test_runs', 'run_id' );
    return $key;
}

=head2 update

Updates this test run with new values supplied by the user.
Accepts a reference to a hash with keys identical to a test run's
fields and values representing the new values entered.
Validation tests should be performed on the values 
before calling this method. If a field is changed, a history 
of that change is logged in the test_run_activity table.

=cut

sub update {
    my $self = shift;
    my ($newvalues) = @_;
    my $dbh = Bugzilla->dbh;
    my $timestamp = Bugzilla::Testopia::Util::get_time_stamp();

    $dbh->bz_lock_tables('test_runs WRITE', 'test_run_activity WRITE',
        'test_fielddefs READ');
    foreach my $field (keys %{$newvalues}){
        if ($self->{$field} ne $newvalues->{$field}){
            $dbh->do("UPDATE test_runs 
                      SET $field = ? WHERE run_id = ?",
                      undef, ($newvalues->{$field}, $self->{'run_id'}));
            # Update the history
            my $field_id = Bugzilla::Testopia::Util::get_field_id($field, "test_runs");
            $dbh->do("INSERT INTO test_run_activity 
                      VALUES(?,?,?,?,?,?)",
                      undef, ($self->{'run_id'}, $field_id, Bugzilla->user->id,
                      $timestamp, $self->{$field}, $newvalues->{$field}));
            $self->{$field} = $newvalues->{$field};
        }
    }
    $dbh->bz_unlock_tables();
}

=head2 update_notes

Updates just the notes for this run

=cut

sub update_notes {
    my $self = shift;
    my ($notes) = @_;
    my $dbh = Bugzilla->dbh;
    $dbh->do("UPDATE test_runs 
              SET notes = ? WHERE run_id = ?",
              undef, $notes, $self->{'run_id'});
}

=head2 clone

Creates a copy of this test run. Accepts the summary of the new run
and the build id to use.

=cut

sub clone {
    my $self = shift;
    my ($summary, $build) = @_;
    my $dbh = Bugzilla->dbh;
    my $timestamp = Bugzilla::Testopia::Util::get_time_stamp();
    $dbh->do("INSERT INTO test_runs ($columns)
              VALUES (?,?,?,?,?,?,?,?,?,?,?)",
              undef, (undef, $self->{'plan_id'}, $self->{'environment_id'}, 
              $self->{'product_version'}, $build, 
              $self->{'plan_text_version'}, $self->{'manager_id'}, 
              $timestamp, undef, $summary, undef));
    my $key = $dbh->bz_last_key( 'test_runs', 'run_id' );
    return $key;   
}

=head2 history

Returns a reference to a list of history entries from the 
test_run_activity table.

=cut

sub history {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my $ref = $dbh->selectall_arrayref(
            "SELECT defs.description AS what, 
                    p.login_name AS who, a.changed, a.oldvalue, a.newvalue
               FROM test_run_activity AS a
               JOIN test_fielddefs AS defs ON a.fieldid = defs.fieldid
               JOIN profiles AS p ON a.who = p.userid
              WHERE a.run_id = ?",
              {'Slice'=>{}}, $self->{'run_id'});

    foreach my $row (@$ref){
        if ($row->{'what'} eq 'Environment'){
            $row->{'oldvalue'} = $self->lookup_environment($row->{'oldvalue'});
            $row->{'newvalue'} = $self->lookup_environment($row->{'newvalue'});
        }
        elsif ($row->{'what'} eq 'Default Build'){
            $row->{'oldvalue'} = $self->lookup_build($row->{'oldvalue'});
            $row->{'newvalue'} = $self->lookup_build($row->{'newvalue'});
        }
        elsif ($row->{'what'} eq 'Manager'){
            $row->{'oldvalue'} = $self->lookup_manager($row->{'oldvalue'});
            $row->{'newvalue'} = $self->lookup_manager($row->{'newvalue'});
        }
    }        
    return $ref;
}

=head2 lookup_environment

Takes an ID of the envionment field and returns the value

=cut

sub lookup_environment {
    my $self = shift;
    my ($id) = @_;
    my $dbh = Bugzilla->dbh;
    my ($value) = $dbh->selectrow_array(
            "SELECT name 
               FROM test_environments
              WHERE environment_id = ?",
              undef, $id);
    return $value;
}

=head2 lookup_build

Takes an ID of the build field and returns the value

=cut

sub lookup_build {
    my $self = shift;
    my ($id) = @_;
    my $dbh = Bugzilla->dbh;
    my ($value) = $dbh->selectrow_array(
            "SELECT name 
               FROM test_builds
              WHERE build_id = ?",
              undef, $id);
    return $value;
}

=head2 lookup_manager

Takes an ID of the manager field and returns the value

=cut

sub lookup_manager {
    my $self = shift;
    my ($id) = @_;
    my $dbh = Bugzilla->dbh;
    my ($value) = $dbh->selectrow_array(
            "SELECT login_name 
               FROM profiles
              WHERE userid = ?",
              undef, $id);
    return $value;
}

=head2 environments

Returns a reference to a list of Testopia::Environment objects.

=cut

sub environments {
    my ($self) = @_;
    my $dbh = Bugzilla->dbh;
    return $self->{'environments'} if exists $self->{'environments'};

    my $environments = 
      $dbh->selectcol_arrayref("SELECT environment_id
                                FROM test_environments");
    
    my @environments;
    foreach my $id (@{$environments}){
        push @environments, Bugzilla::Testopia::Environment->new($id);
    }
    $self->{'environments'} = \@environments;
    return $self->{'environments'};
}

=head2 get_status_list

Returns a list of statuses for a run

=cut

sub get_status_list {
    my @status = (
        { 'id' => 0, 'name' => 'Running' },
        { 'id' => 1, 'name' => 'Stopped' },
    );
    return \@status;
}

=head2 get_distinct_builds

Returns a list of build names for use in searches

=cut

sub get_distinct_builds {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my $query = "SELECT build.name AS id, build.name " .
                  "FROM test_builds AS build " .
                  "JOIN products ON build.product_id = products.id " .
             "LEFT JOIN group_control_map " .
              "ON group_control_map.product_id = products.id ";
    if (Param('useentrygroupdefault')) {
        $query .= "AND group_control_map.entry != 0 ";
    } else {
        $query .= "AND group_control_map.membercontrol = " .
              CONTROLMAPMANDATORY . " ";
    }
    if (%{Bugzilla->user->groups}) {
        $query .= "AND group_id NOT IN(" . 
              join(',', values(%{Bugzilla->user->groups})) . ") ";
    }
    $query .= "WHERE group_id IS NULL ORDER BY build.name";
    
    my $ref = $dbh->selectall_arrayref($query, {'Slice'=>{}});

    return $ref;             
}

=head2 get_distinct_milestones

Returns a list of milestones for use in searches

=cut

sub get_distinct_milestones {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my $ref = $dbh->selectall_arrayref(
            "SELECT DISTINCT value AS id, value as name
             FROM milestones
             ORDER BY sortkey", {'Slice'=>{}});

    return $ref;             
}

=head2 get_environments

Returns a list of environments for use in searches

=cut

sub get_environments {
    my $dbh = Bugzilla->dbh;
    my $ref = $dbh->selectall_arrayref(
                "SELECT environment_id AS id, name
                 FROM test_environments
                 ORDER BY name",
                 {'Slice'=>{}});
                 
    return $ref;
}

=head2 canedit

Returns true if the logged in user has rights to edit this test run.

=cut

sub canedit {
    my $self = shift;
    return $self->canview 
    && (UserInGroup('managetestplans') 
      || UserInGroup('edittestcases')
        || UserInGroup('runtests'));
}

=head2 canview

Returns true if the logged in user has rights to view this test run.

=cut

sub canview {
    my $self = shift;
    return $self->{'canview'} if exists $self->{'canview'};
    $self->{'canview'} = Bugzilla::Testopia::Util::can_view_product($self->plan->product_id);
    return $self->{'canview'};
}

=head2 candelete

Returns true if the logged in user has rights to delete this test run.

=cut

sub candelete {
    my $self = shift;
    return $self->canedit && Param('allow-test-deletion');
}

###############################
####      Accessors        ####
###############################

=head1 ACCESSOR METHODS

=head2 id

Returns the ID for this object

=head2 plan_text_version

Returns the plan's text version of this run

=head2 plan_id

Returns the  plan idof this run

=head2 environment_id

Returns the environment id of this run

=head2 manager

Returns a Bugzilla::User object representing the run's manager

=head2 start_date

Returns the time stamp of when this run was started

=head2 stop_date

Returns the  time stamp of when this run was completed

=head2 summary

Returns the summary of this run

=head2 notes

Returns the notes for this run

=head2 product_version

Returns the product version of this run

=cut

sub id                { return $_[0]->{'run_id'};          }
sub plan_text_version { return $_[0]->{'plan_text_version'};  }
sub plan_id           { return $_[0]->{'plan_id'};  }
sub environment_id    { return $_[0]->{'environment_id'};  }
sub manager           { return Bugzilla::User->new($_[0]->{'manager_id'});  }
sub start_date        { return $_[0]->{'start_date'};        }
sub stop_date         { return $_[0]->{'stop_date'}; }
sub summary           { return $_[0]->{'summary'};  }
sub notes             { return $_[0]->{'notes'};  }
sub product_version   { return $_[0]->{'product_version'};  }

=head2 plan

Returns the Testopia::TestPlan object of the plan this run 
is assoceated with

=cut

sub plan {
    my $self = shift;
    return $self->{'plan'} if exists $self->{'plan'};
    $self->{'plan'} = Bugzilla::Testopia::TestPlan->new($self->{'plan_id'});
    return $self->{'plan'};
}

=head2 tags

Returns a reference to a list of Testopia::TestTag objects 
associated with this run

=cut

sub tags {
    my ($self) = @_;
    my $dbh = Bugzilla->dbh;
    return $self->{'tags'} if exists $self->{'tags'};
    my $tagids = $dbh->selectcol_arrayref("SELECT tag_id 
                                          FROM test_run_tags
                                          WHERE run_id = ?", 
                                          undef, $self->{'run_id'});
    my @tags;
    foreach my $t (@{$tagids}){
        push @tags, Bugzilla::Testopia::TestTag->new($t);
    }
    $self->{'tags'} = \@tags;
    return $self->{'tags'};
}

=head2 environment

Returns the Testopia::Environment object of the environment 
this run is assoceated with

=cut

sub environment {
    my $self = shift;
    return $self->{'environment'} if exists $self->{'environment'};
    $self->{'environment'} = Bugzilla::Testopia::Environment->new($self->{'environment_id'});
    return $self->{'environment'};
    
}

=head2 build

Returns the Testopia::Build object of the plan this run 
is assoceated with

=cut

sub build {
    my $self = shift;
    return $self->{'build'} if exists $self->{'build'};
    $self->{'build'} = Bugzilla::Testopia::Build->new($self->{'build_id'});
    return $self->{'build'};
    
}

=head2 runtime

Returns the total time the run took to complete

=cut

sub runtime {
    
}

=head2 bugs

Returns a reference to a list of Bugzilla::Bug objects associated
with this run

=cut

sub bugs {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    return $self->{'bugs'} if exists $self->{'bugs'};
    my $ref = $dbh->selectcol_arrayref(
          "SELECT bug_id
             FROM test_case_bugs b
             JOIN test_case_runs r ON r.case_run_id = b.case_run_id
            WHERE r.run_id = ?", 
           undef, $self->{'run_id'});
    my @bugs;
    foreach my $id (@{$ref}){
        push @bugs, Bugzilla::Bug->new($id, Bugzilla->user->id);
    }
    $self->{'bugs'} = \@bugs;
    return $self->{'bugs'};
}

=head2 cc

Returns a reference to a list of Bugzilla::User objects
on the CC list of this run

=cut

sub cc {
    my $self = shift;
    return $self->{'cc'} if exists $self->{'cc'};
    my $dbh = Bugzilla->dbh;
    my $ref = $dbh->selectcol_arrayref(
        "SELECT who FROM test_run_cc 
         WHERE run_id=?", undef, $self->{'run_id'});
    my @cc;     
    foreach my $id (@{$ref}){
        push @cc, Bugzilla::User->new($id);
    }
    $self->{'cc'} = \@cc;
    return $self->{'cc'};
}

=head2 cases

Returns a reference to a list of Testopia::TestCase objects 
associated with this run

=cut

sub cases {
    my $self = shift;
    return $self->{'cases'} if exists $self->{'cases'};
    my @cases;
    foreach my $cr (@{$self->current_caseruns}){
        push @cases, Bugzilla::Testopia::TestCase->new($cr->id);
    }
    $self->{'cases'} = \@cases;
    return $self->{'cases'};
    
}

=head2 case_count

Returns a count of the test cases associated with this run

=cut

sub case_count {
    my $self = shift;
    return scalar @{$self->cases};
}

=head2 idle_count

Returns a count of the number of case-runs in this run with a status
of IDLE

=cut

sub idle_count {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    my ($count) = $dbh->selectrow_array(
           "SELECT COUNT(*) 
              FROM test_case_runs cr 
              JOIN test_case_run_status cs 
                ON cr.case_run_status_id = cs.case_run_status_id 
             WHERE cs.name = ? AND cr.run_id = ? AND cr.iscurrent = 1",
            undef, ('IDLE', $self->{'run_id'}));

    $self->{'idle_count'}   = $count;
    return $self->{'idle_count'};
}

=head2 passed_count

Returns a count of the number of case-runs in this run with a status
of PASSED

=cut

sub passed_count {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    my ($count) = $dbh->selectrow_array(
           "SELECT COUNT(*) 
              FROM test_case_runs cr 
              JOIN test_case_run_status cs 
                ON cr.case_run_status_id = cs.case_run_status_id 
             WHERE cs.name = ? AND cr.run_id = ? AND cr.iscurrent = 1",
            undef, ('PASSED', $self->{'run_id'}));

    $self->{'passed_count'} = $count;   
    return $self->{'passed_count'}; 
}

=head2 failed_count

Returns a count of the number of case-runs in this run with a status
of FAILED

=cut

sub failed_count {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    my ($count) = $dbh->selectrow_array(
           "SELECT COUNT(*) 
              FROM test_case_runs cr 
              JOIN test_case_run_status cs 
                ON cr.case_run_status_id = cs.case_run_status_id 
             WHERE cs.name = ? AND cr.run_id = ? AND cr.iscurrent = 1",
            undef, ('FAILED', $self->{'run_id'}));

    $self->{'failed_count'} = $count;
    return $self->{'failed_count'};
}

=head2 running_count

Returns a count of the number of case-runs in this run with a status
of RUNNING

=cut

sub running_count {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    my ($count) = $dbh->selectrow_array(
           "SELECT COUNT(*) 
              FROM test_case_runs cr 
              JOIN test_case_run_status cs 
                ON cr.case_run_status_id = cs.case_run_status_id 
             WHERE cs.name = ? AND cr.run_id = ? AND cr.iscurrent = 1",
            undef, ('RUNNING', $self->{'run_id'}));

    $self->{'running_count'} = $count;
    return $self->{'running_count'}; 
}

=head2 paused_count

Returns a count of the number of case-runs in this run with a status
of PAUSED

=cut

sub paused_count {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    my ($count) = $dbh->selectrow_array(
           "SELECT COUNT(*) 
              FROM test_case_runs cr 
              JOIN test_case_run_status cs 
                ON cr.case_run_status_id = cs.case_run_status_id 
             WHERE cs.name = ? AND cr.run_id = ? AND cr.iscurrent = 1",
            undef, ('PAUSED', $self->{'run_id'}));

    $self->{'paused_count'} = $count;
    return $self->{'paused_count'};
}

=head2 blocked_count

Returns a count of the number of case-runs in this run with a status
of BLOCKED

=cut

sub blocked_count {
    my $self = shift;
    my $dbh = Bugzilla->dbh;

    my ($count) = $dbh->selectrow_array(
           "SELECT COUNT(*) 
              FROM test_case_runs cr 
              JOIN test_case_run_status cs 
                ON cr.case_run_status_id = cs.case_run_status_id 
             WHERE cs.name = ? AND cr.run_id = ? AND cr.iscurrent = 1",
            undef, ('BLOCKED', $self->{'run_id'}));

    $self->{'blocked_count'} = $count;
    return $self->{'blocked_count'};
}

=head2 percent_complete

Returns a number representing the percentage of case-runs
that have a status vs. those with a status of IDLE

=cut

sub percent_complete {    
    my $self = shift;
    my $notrun = $self->idle_count + $self->running_count + $self->paused_count;
    my $run = $self->passed_count + $self->failed_count + $self->blocked_count;
    $self->{'percent_complete'} = calculate_percent_completed($notrun, $run); 
    return $self->{'percent_complete'};
}

=head2 current_caseruns

Returns a reference to a list of TestCaseRun objects that are the
current case-runs on this run

=cut

sub current_caseruns {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    return $self->{'current_caseruns'} if exists $self->{'current_caseruns'};
    my $ref = $dbh->selectcol_arrayref(
        "SELECT case_run_id FROM test_case_runs
         WHERE run_id=? AND iscurrent=1", undef,
         $self->{'run_id'});
    my @caseruns;
    
    foreach my $id (@{$ref}){
        push @caseruns, Bugzilla::Testopia::TestCaseRun->new($id);
    }
    $self->{'current_caseruns'} = \@caseruns;
    return $self->{'current_caseruns'};
}

=head2 case_id_list

Returns a list of case_id's from the current case runs.

=cut

sub case_id_list {
    my $self = shift;
    my @ids;
    foreach my $c (@{$self->current_caseruns}){
        push @ids, $c->case_id;
    }
    
    return join(",", @ids);
}

=head1 SEE ALSO

Testopia::(TestPlan, TestCase, Category, Build, Environment)

=head1 AUTHOR

Greg Hendricks <ghendricks@novell.com>

=cut

1;
