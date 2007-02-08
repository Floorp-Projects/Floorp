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
use Bugzilla::Bug;

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

sub report_columns {
    my $self = shift;
    my %columns;
    # Changes here need to match Report.pm
    $columns{'Status'}        = "run_status";        
    $columns{'Version'}       = "default_product_version";
    $columns{'Product'}       = "product";
    $columns{'Build'}         = "build";
    $columns{'Milestone'}     = "milestone";
    $columns{'Environment'}   = "environment";
    $columns{'Tags'}          = "tags";
    $columns{'Manager'}       = "manager";
    my @result;
    push @result, {'name' => $_, 'id' => $columns{$_}} foreach (sort(keys %columns));
    unshift @result, {'name' => '<none>', 'id'=> ''};
    return \@result;     
        
}

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
    $percent = 0;
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
    $dbh->do("INSERT INTO test_run_tags(tag_id, run_id, userid) VALUES(?,?,?)",
              undef, $tag_id, $self->{'run_id'}, Bugzilla->user->id);
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
    return 0 if $self->check_case($case_id);
    my $case = Bugzilla::Testopia::TestCase->new($case_id);
    return 0 if $case->status ne 'CONFIRMED';
    my $caserun = Bugzilla::Testopia::TestCaseRun->new({
        'run_id' => $self->{'run_id'},
        'case_id' => $case_id,
        'assignee' => $case->default_tester->id,
        'case_text_version' => $case->version,
        'build_id' => $self->build->id,
        'environment_id' => $self->environment_id,
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
            (run_id, fieldid, who, changed, oldvalue, newvalue)
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
    my ($summary, $manager, $plan_id, $build) = @_;
    my $dbh = Bugzilla->dbh;
    my $timestamp = Bugzilla::Testopia::Util::get_time_stamp();
    $dbh->do("INSERT INTO test_runs ($columns)
              VALUES (?,?,?,?,?,?,?,?,?,?,?)",
              undef, (undef, $plan_id, $self->{'environment_id'}, 
              $self->{'product_version'}, $build, 
              $self->{'plan_text_version'}, $manager, 
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

=head2 obliterate

Removes this run and all things that reference it.

=cut

sub obliterate {
    my $self = shift;
    my ($cgi, $template) = @_;
    my $dbh = Bugzilla->dbh;
    my $vars;
    
    my $progress_interval = 500;
    my $i = 0;
    my $total = scalar @{$self->caseruns};
    
    foreach my $obj (@{$self->caseruns}){
        $i++;
        if ($cgi && $i % $progress_interval == 0){
            print $cgi->multipart_end;
            print $cgi->multipart_start;
            $vars->{'complete'} = $i;
            $vars->{'total'} = $total;
            $vars->{'process'} = "Deleting Run " . $self->id;
        
            $template->process("testopia/progress.html.tmpl", $vars)
              || ThrowTemplateError($template->error());
        }
    
        $obj->obliterate;
    }

    $dbh->do("DELETE FROM test_run_cc WHERE run_id = ?", undef, $self->id);
    $dbh->do("DELETE FROM test_run_tags WHERE run_id = ?", undef, $self->id);
    $dbh->do("DELETE FROM test_run_activity WHERE run_id = ?", undef, $self->id);
    $dbh->do("DELETE FROM test_runs WHERE run_id = ?", undef, $self->id);
    return 1;
}

=head2 Check_case

Checks if the given test case is already associated with this run

=cut

sub check_case {
    my $self = shift;
    my ($case_id) = @_;
    my $dbh = Bugzilla->dbh;
    my ($value) = $dbh->selectrow_array(
            "SELECT case_run_id 
               FROM test_case_runs
              WHERE case_id = ? AND run_id = ?",
              undef, ($case_id, $self->{'run_id'}));
    return $value;
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

=head2 lookup_environment_by_name

Takes the name of an envionment and returns its id

=cut

sub lookup_environment_by_name {
    my ($name) = @_;
    my $dbh = Bugzilla->dbh;
    my ($value) = $dbh->selectrow_array(
            "SELECT environment_id 
               FROM test_environments
              WHERE name = ?",
              undef, $name);
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

=head2 last_changed

Returns the date of the last change in the history table

=cut

sub last_changed {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    
    my ($date) = $dbh->selectrow_array(
            "SELECT MAX(changed)
               FROM test_run_activity 
              WHERE run_id = ?",
              undef, $self->id);

    return $self->{'creation_date'} unless $date;
    return $date;
}

sub filter_case_categories {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    
    my $ids = $dbh->selectcol_arrayref(
            "SELECT DISTINCT tcc.category_id
               FROM test_case_categories AS tcc
               JOIN test_cases ON test_cases.category_id = tcc.category_id
               JOIN test_case_runs AS tcr ON test_cases.case_id = tcr.case_id  
              WHERE run_id = ?",
              undef, $self->id);
    
    my @categories;
    foreach my $id (@$ids){
        push @categories, Bugzilla::Testopia::Category->new($id);
    }
    
    return \@categories;
}

sub filter_builds {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    
    my $ids = $dbh->selectcol_arrayref(
            "SELECT DISTINCT build_id
               FROM test_case_runs
              WHERE run_id = ?",
              undef, $self->id);
    
    my @builds;
    foreach my $id (@$ids){
        push @builds, Bugzilla::Testopia::Build->new($id);
    }
    return \@builds;
}

sub filter_components {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    
    my $ids = $dbh->selectcol_arrayref(
            "SELECT DISTINCT components.id
               FROM components
               JOIN test_case_components AS tcc ON tcc.component_id = components.id
               JOIN test_cases ON test_cases.case_id = tcc.case_id
               JOIN test_case_runs AS tcr ON test_cases.case_id = tcr.case_id  
              WHERE run_id = ?",
              undef, $self->id);
    
    my @components;
    foreach my $id (@$ids){
        push @components, Bugzilla::Component->new($id);
    }
    
    return \@components;
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
                "SELECT DISTINCT name AS id, name
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
    return 1 if Bugzilla->user->in_group('Testers');
    return 1 if $self->plan->get_user_rights(Bugzilla->user->id, GRANT_REGEXP) & 2;
    return 1 if $self->plan->get_user_rights(Bugzilla->user->id, GRANT_DIRECT) & 2;
    return 0;
}

=head2 canview

Returns true if the logged in user has rights to view this test run.

=cut

sub canview {
    my $self = shift;
    return 1 if Bugzilla->user->in_group('Testers');
    return 1 if $self->plan->get_user_rights(Bugzilla->user->id, GRANT_REGEXP) > 0;
    return 1 if $self->plan->get_user_rights(Bugzilla->user->id, GRANT_DIRECT) > 0;
    return 0;

}

=head2 candelete

Returns true if the logged in user has rights to delete this test run.

=cut

sub candelete {
    my $self = shift;
    return 1 if Bugzilla->user->in_group('admin');
    return 0 unless Param("allow-test-deletion");
    return 1 if Bugzilla->user->in_group('Testers') && Param("testopia-allow-group-member-deletes");
    return 1 if $self->plan->get_user_rights(Bugzilla->user->id, GRANT_REGEXP) & 4;
    return 1 if $self->plan->get_user_rights(Bugzilla->user->id, GRANT_DIRECT) & 4;
    return 0;
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

=head2 type

Returns 'case'

=cut

sub type {
    my $self = shift;
    $self->{'type'} = 'run';
    return $self->{'type'};
}

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
          "SELECT DISTINCT bug_id
             FROM test_case_bugs b
             JOIN test_case_runs r ON r.case_run_id = b.case_run_id
            WHERE r.run_id = ?", 
           undef, $self->{'run_id'});
    my @bugs;
    foreach my $id (@{$ref}){
        push @bugs, Bugzilla::Bug->new($id, Bugzilla->user->id);
    }
    $self->{'bugs'} = \@bugs if @bugs;
    $self->{'bug_list'} = join(',', @$ref);
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
        push @cases, Bugzilla::Testopia::TestCase->new($cr->case_id);
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

sub case_run_count {
    my $self = shift;
    my ($status_id) = @_;
    my $dbh = Bugzilla->dbh;
    my $query = 
           "SELECT COUNT(*) 
              FROM test_case_runs 
             WHERE run_id = ? AND iscurrent = 1";
    $query .= " AND case_run_status_id = ?" if $status_id;
    
    my $count;
    if ($status_id){
        ($count) = $dbh->selectrow_array($query,undef,($self->{'run_id'}, $status_id));
    }
    else {
        ($count) = $dbh->selectrow_array($query,undef,$self->{'run_id'});
    }
    
    return $count;       
}

#TODO: Replace these with case_run_count
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

sub percent_passed {    
    my $self = shift;
    my $notrun = $self->idle_count + $self->running_count + $self->paused_count + $self->failed_count + $self->blocked_count;
    my $run = $self->passed_count;
    $self->{'percent_passed'} = calculate_percent_completed($notrun, $run); 
    return $self->{'percent_passed'};
}

sub percent_failed {    
    my $self = shift;
    my $notrun = $self->idle_count + $self->running_count + $self->paused_count + $self->passed_count + $self->blocked_count;
    my $run = $self->failed_count;
    $self->{'percent_failed'} = calculate_percent_completed($notrun, $run); 
    return $self->{'percent_failed'};
}

sub percent_blocked {    
    my $self = shift;
    my $notrun = $self->idle_count + $self->running_count + $self->paused_count + $self->passed_count + $self->failed_count;
    my $run = $self->blocked_count;
    $self->{'percent_blocked'} = calculate_percent_completed($notrun, $run); 
    return $self->{'percent_blocked'};
}

sub percent_not_run {    
    my $self = shift;
    my $notrun = $self->failed_count + $self->running_count + $self->paused_count + $self->passed_count;
    my $run = $self->idle_count + $self->blocked_count;
    $self->{'percent_not_run'} = calculate_percent_completed($notrun, $run); 
    return $self->{'percent_not_run'};
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

=head2 caseruns

Returns a reference to a list of TestCaseRun objects that belong
to this run

=cut

sub caseruns {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    return $self->{'caseruns'} if exists $self->{'caseruns'};
    my $ref = $dbh->selectcol_arrayref(
        "SELECT case_run_id FROM test_case_runs
         WHERE run_id=?", undef, $self->{'run_id'});
    my @caseruns;
    
    foreach my $id (@{$ref}){
        push @caseruns, Bugzilla::Testopia::TestCaseRun->new($id);
    }
    $self->{'caseruns'} = \@caseruns;
    return $self->{'caseruns'};
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
