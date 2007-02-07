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
# Portions created by Greg Hendricks are Copyright (C) 2006
# Novell. All Rights Reserved.
#
# Contributor(s): Greg Hendricks <ghendricks@novell.com>
=head1 NAME

Bugzilla::Testopia::TestCaseRun - Testopia Test Case Run object

=head1 DESCRIPTION

This module represents a test case run in Testopia. 
A test case run is a record in the test_case_runs table which joins
test cases to test runs. Basically, for each test run a selction of 
test cases is made to be included in that run. As a test run 
progresses, testers set statuses on each of the cases in the run.
If the build is changed on a case-run with a status, a clone of that
case-run is made in the table for historical purposes.

=head1 SYNOPSIS

use Bugzilla::Testopia::TestCaseRun;

 $caserun = Bugzilla::Testopia::TestCaseRun->new($caserun_id);
 $caserun = Bugzilla::Testopia::TestCaseRun->new(\%caserun_hash);

=cut

package Bugzilla::Testopia::TestCaseRun;

use strict;

use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::User;
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::Constants;
use Bugzilla::Bug;

use Date::Format;


###############################
####    Initialization     ####
###############################

=head1 FIELDS

    case_run_id
    run_id
    case_id
    assignee
    testedby
    case_run_status_id
    case_text_version
    build_id
    environment_id
    notes
    close_date
    iscurrent
    sortkey

=cut

use constant DB_COLUMNS => qw(
    case_run_id
    run_id
    case_id
    assignee
    testedby
    case_run_status_id
    case_text_version
    build_id
    environment_id
    notes
    close_date
    iscurrent
    sortkey
);

our $columns = join(", ", DB_COLUMNS);

sub report_columns {
    my $self = shift;
    my %columns;
    # Changes here need to match Report.pm
    $columns{'Build'}           = "build";
    $columns{'Status'}          = "status";        
    $columns{'Environment'}     = "environment";
    $columns{'Assignee'}        = "assignee";
    $columns{'Tested By'}       = "testedby";
    $columns{'Milestone'}       = "milestone";
    $columns{'Case Tags'}       = "case_tags";
    $columns{'Run Tags'}        = "run_tags";
    $columns{'Requirement'}     = "requirement";
    $columns{'Priority'}        = "priority";
    $columns{'Default tester'}  = "default_tester";
    $columns{'Category'}        = "category";
    $columns{'Component'}       = "component";
    my @result;
    push @result, {'name' => $_, 'id' => $columns{$_}} foreach (sort(keys %columns));
    unshift @result, {'name' => '<none>', 'id'=> ''};
    return \@result;     
        
}

###############################
####       Methods         ####
###############################

=head1 METHODS

=head2 new

Instantiate a new case run. This takes a single argument 
either a test case ID or a reference to a hash containing keys 
identical to a test case-run's fields and desired values.

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
    my ($param, $run_id, $build_id, $env_id) = (@_);
    my $dbh = Bugzilla->dbh;

    my $id = $param unless (ref $param eq 'HASH');
    my $obj;

    if (defined $id && detaint_natural($id) && !$run_id) {

        $obj = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM test_case_runs
            WHERE case_run_id = ?}, undef, $id);
    
    } elsif ($run_id && detaint_natural($run_id) 
             && $build_id && detaint_natural($build_id) 
             && $env_id && detaint_natural($env_id)){
                 
         my $case_id = $param;
         detaint_natural($case_id) || return undef;
         $obj = $dbh->selectrow_hashref(
            "SELECT $columns FROM test_case_runs
             WHERE case_id = ?
               AND run_id = ?
               AND build_id = ?
               AND environment_id = ?", 
             undef, ($case_id, $run_id, $build_id, $env_id));
                  
    } elsif (ref $param eq 'HASH'){
         $obj = $param;   

    } else {
        Bugzilla::Error::ThrowCodeError('bad_arg',
            {argument => 'param',
             function => 'Testopia::TestCaseRun::_init'});
    }

    return undef unless (defined $obj);

    foreach my $field (keys %$obj) {
        $self->{$field} = $obj->{$field};
    }

    return $self;
}

=head2 store

Stores a test case run object in the database. This method is used 
to store a newly created test case run. It returns the new id.

=cut

sub store {
    my $self = shift;
    my $dbh = Bugzilla->dbh;    
    $dbh->do("INSERT INTO test_case_runs ($columns)
              VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)", undef,
              (undef, $self->{'run_id'}, $self->{'case_id'}, $self->{'assignee'},
               undef, IDLE, $self->{'case_text_version'}, 
               $self->{'build_id'}, $self->{'environment_id'}, 
               undef, undef, 1, 0));

    my $key = $dbh->bz_last_key( 'test_case_runs', 'case_run_id' );
    return $key;               
}

=head2 clone

Creates a copy of this caserun and sets it as the current record

=cut

sub clone {
    my $self = shift;
    my ($build_id, $env_id ,$run_id, $case_id) = @_;
    $run_id   ||= $self->{'run_id'};
    $case_id  ||= $self->{'case_id'};
    
    my $dbh = Bugzilla->dbh;    
    $dbh->do("INSERT INTO test_case_runs ($columns)
              VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)", undef,
              (undef, $run_id, $case_id, $self->{'assignee'},
               undef, IDLE, $self->{'case_text_version'}, 
               $build_id, $env_id, 
               undef, undef, 1, 0));

    my $key = $dbh->bz_last_key( 'test_case_runs', 'case_run_id' );
    
    $self->set_as_current($key);
    return $key;
}

=head2 check_exists

Checks for an existing entry with the same build and environment for this 
case and run and switches self to that object.

=cut

sub switch {
   my $self = shift;
   my ($build_id, $env_id ,$run_id, $case_id) = @_;

   $run_id   ||= $self->{'run_id'};
   $case_id  ||= $self->{'case_id'};
   $build_id ||= $self->{'build_id'};
   $env_id   ||= $self->{'environment_id'};
   
   my $dbh = Bugzilla->dbh;     
   my ($is) = $dbh->selectrow_array(
        "SELECT case_run_id 
           FROM test_case_runs 
          WHERE run_id = ? 
            AND case_id = ? 
            AND build_id = ?
            AND environment_id = ?",
          undef, ($run_id, $case_id, $build_id, $env_id));

   if ($is){
       $self = Bugzilla::Testopia::TestCaseRun->new($is);
   }
   else {
       my $oldbuild = $self->{'build_id'};
       my $oldenv = $self->{'environment_id'};
       
       $self = Bugzilla::Testopia::TestCaseRun->new($self->clone($build_id,$env_id));
       
       if ($oldbuild != $build_id){
           my $build = Bugzilla::Testopia::Build->new($build_id);
           my $note  = "Build Changed by ". Bugzilla->user->login; 
              $note .= ". Old build: '". $self->build->name;
              $note .= "' New build: '". $build->name;
              $note .= "'. Resetting to IDLE.";
           $self->append_note($note);
       }
       if ($oldenv != $env_id){
           my $environment = Bugzilla::Testopia::Environment->new($env_id);
           my $note  = "Environment Changed by ". Bugzilla->user->login;
              $note .= ". Old environment: '". $self->environment->name;
              $note .= "' New environment: '". $environment->name;
              $note .= "'. Resetting to IDLE.";
           $self->append_note($note);
       }
   }
   $self->set_as_current; 
   return $self;
}

=head2 _update_fields

Update this case-run in the database if a change is made to an 
updatable field.

=cut

sub _update_fields{
    my $self = shift;
    my ($newvalues) = @_;
    my $dbh = Bugzilla->dbh;

    if ($newvalues->{'case_run_status_id'} && $newvalues->{'case_run_status_id'} == FAILED){
        $self->_update_deps(BLOCKED);
    }
    elsif ($newvalues->{'case_run_status_id'} && $newvalues->{'case_run_status_id'} == PASSED){
        $self->_update_deps(IDLE);
    }

    $dbh->bz_lock_tables('test_case_runs WRITE');
    foreach my $field (keys %{$newvalues}){
        $dbh->do("UPDATE test_case_runs 
                  SET $field = ? WHERE case_run_id = ?",
                  undef, $newvalues->{$field}, $self->{'case_run_id'});
    }
    $dbh->bz_unlock_tables();

    return $self->{'case_run_id'};   
}

=head2 set_as_current

Sets this case-run as the current or active one in the history
list of case-runs of this build and case_id

=cut

sub set_as_current {
    my $self = shift;
    my ($caserun) = @_;
    $caserun = $self->{'case_run_id'} unless defined $caserun;
    my $dbh = Bugzilla->dbh;
    my $list = $self->get_case_run_list;

    $dbh->bz_lock_tables('test_case_runs WRITE');
    foreach my $c (@{$list}){
        $dbh->do("UPDATE test_case_runs
                  SET iscurrent = 0
                  WHERE case_run_id = ?",
                  undef, $c);
    }
    $dbh->do("UPDATE test_case_runs
              SET iscurrent = 1
              WHERE case_run_id = ?",
              undef, $caserun);
    $dbh->bz_unlock_tables();
}

=head2 set_build

Sets the build on a case-run

=cut

sub set_build {
    my $self = shift;
    my ($build_id) = @_;
    $self->_update_fields({'build_id' => $build_id});
    $self->{'build_id'} = $build_id;
}

=head2 set_environment

Sets the environment on a case-run

=cut

sub set_environment {
    my $self = shift;
    my ($env_id) = @_;
    $self->_update_fields({'environment_id' => $env_id});
    $self->{'environment_id'} = $env_id;
}

=head2 set_status

Sets the status on a case-run and updates the close_date and testedby 
if the status is a closed status.

=cut

sub set_status {
    my $self = shift;
    my ($status_id) = @_;
    
    my $oldstatus = $self->status;
    my $newstatus = $self->lookup_status($status_id);
    
    $self->_update_fields({'case_run_status_id' => $status_id});
    if ($status_id == IDLE){
        $self->_update_fields({'close_date' => undef});
        $self->_update_fields({'testedby' => undef});
        $self->{'close_date'} = undef;
        $self->{'testedby'} = undef;
    }
    elsif ($status_id == RUNNING || $status_id == PAUSED){
        $self->_update_fields({'close_date' => undef});
        $self->{'close_date'} = undef;
    }
    else {
        my $timestamp = Bugzilla::Testopia::Util::get_time_stamp();
        $self->_update_fields({'close_date' => $timestamp});
        $self->_update_fields({'testedby' => Bugzilla->user->id});
        $self->{'close_date'} = $timestamp;
        $self->{'testedby'} = Bugzilla->user->id;
        $self->update_bugs('REOPENED') if ($status_id == FAILED);
        $self->update_bugs('VERIFIED') if ($status_id == PASSED);
    }
    
    my $note = "Status changed from $oldstatus to $newstatus by ". Bugzilla->user->login;
    $note .= " for build '". $self->build->name ."' and environment '". $self->environment->name; 
    $self->append_note($note);
    $self->{'case_run_status_id'} = $status_id;
    $self->{'status'} = undef;
}

=head2 set_assignee

Sets the assigned tester for the case-run

=cut

sub set_assignee {
    my $self = shift;
    my ($user_id) = @_;
    
    my $oldassignee = $self->assignee->login;
    my $newassignee = Bugzilla::User->new($user_id);
    
    $self->_update_fields({'assignee' => $user_id});
    $self->{'assignee'} = $newassignee;
    
    my $note = "Assignee changed from $oldassignee to ". $newassignee->login;
    $note   .= " by ". Bugzilla->user->login;
    $note   .= " for build '". $self->build->name;
    $note   .= "' and environment '". $self->environment->name;
    $self->append_note($note);
}

=head2 lookup_status

Returns the status name of the given case_run_status_id

=cut

sub lookup_status {
    my $self = shift;
    my ($status_id) = @_;
    my $dbh = Bugzilla->dbh;
    my ($status) = $dbh->selectrow_array(
            "SELECT name 
               FROM test_case_run_status 
              WHERE case_run_status_id = ?",
              undef, $status_id);
   return $status;
}

=head2 lookup_status_by_name

Returns the id of the status name passed.

=cut

sub lookup_status_by_name {
    my ($name) = @_;
    my $dbh = Bugzilla->dbh;
    
    my ($value) = $dbh->selectrow_array(
            "SELECT case_run_status_id
             FROM test_case_run_status
             WHERE name = ?",
             undef, $name);
    return $value;
}

=head2 append_note

Updates the notes field for the case-run

=cut

sub append_note {
    my $self = shift;
    my ($note) = @_;
    return unless $note;
    my $timestamp = time2str("%c", time());
    $note = "$timestamp: $note";
    if ($self->{'notes'}){
        $note = $self->{'notes'} . "\n" . $note;
    }
    $self->_update_fields({'notes' => $note});
    $self->{'notes'} = $note;
}

=head2 _update_deps

Private method for updating blocked test cases. If the pre-requisite 
case fails, the blocked test cases in a run get a status of BLOCKED
if it passes they are set back to IDLE. This only happens to the
current case run and only if it doesn't already have a closed status.
=cut
 
sub _update_deps {
    my $self = shift;
    my ($status) = @_;
    my $deplist = $self->case->get_dep_tree;
    return unless $deplist;
    
    my $dbh = Bugzilla->dbh;    
    $dbh->bz_lock_tables("test_case_runs WRITE");
    my $caseruns = $dbh->selectcol_arrayref(
       "SELECT case_run_id 
          FROM test_case_runs    
         WHERE iscurrent = 1 
           AND run_id = ? 
           AND case_run_status_id IN(". join(',', (IDLE,RUNNING,PAUSED,BLOCKED)) .") 
           AND case_id IN (". join(',', @$deplist) .")",
           undef, $self->{'run_id'});
    my $sth = $dbh->prepare_cached(
        "UPDATE test_case_runs 
         SET case_run_status_id = ?
       WHERE case_run_id = ?");
    
    foreach my $id (@$caseruns){
        $sth->execute($status, $id);
    }
    $dbh->bz_unlock_tables;
    
    $self->{'updated_deps'} = $caseruns;
}

=head2 get_case_run_list

Returns a reference to a list of case-runs for the given case and run

=cut

sub get_case_run_list {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my $ref = $dbh->selectcol_arrayref(
            "SELECT case_run_id FROM test_case_runs
             WHERE case_id = ? AND run_id = ?", undef,
             ($self->{'case_id'}, $self->{'run_id'}));

    return $ref;    
}

=head2 get_status_list

Returns a list reference of the legal statuses for a test case-run

=cut

sub get_status_list {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my $ref = $dbh->selectall_arrayref(
            "SELECT case_run_status_id AS id, name 
             FROM test_case_run_status
             ORDER BY sortkey", {'Slice' =>{}});

    return $ref    
}

=head2 attach_bug

Attaches the specified bug to this test case-run

=cut

sub attach_bug {
    my $self = shift;
    my ($bug, $caserun_id) = @_;
    $caserun_id ||= $self->{'case_run_id'};
    my $dbh = Bugzilla->dbh;

    $dbh->bz_lock_tables('test_case_bugs WRITE');
    my ($exists) = $dbh->selectrow_array(
            "SELECT bug_id 
               FROM test_case_bugs 
              WHERE case_run_id=?
                AND bug_id=?", 
             undef, ($caserun_id, $bug));
    if ($exists) {
        $dbh->bz_unlock_tables();
        return;
    }
    my ($check) = $dbh->selectrow_array(
            "SELECT bug_id 
               FROM test_case_bugs 
              WHERE case_id=?
                AND bug_id=?
                AND case_run_id=?", 
             undef, ($caserun_id, $bug, undef));
             
    if ($check){
        $dbh->do("UPDATE test_case_bugs 
                     SET test_case_run_id = ?
                   WHERE case_id = ?
                     AND bug_id = ?", 
                 undef, ($bug, $self->{'case_run_id'}));
    }
    else{
        $dbh->do("INSERT INTO test_case_bugs (bug_id, case_run_id, case_id)
                  VALUES(?,?,?)", undef, ($bug, $self->{'case_run_id'}, $self->{'case_id'}));
    }
    $dbh->bz_unlock_tables();
}

=head2 detach_bug

Removes the association of the specified bug from this test case-run

=cut

sub detach_bug {
    my $self = shift;
    my ($bug) = @_;
    my $dbh = Bugzilla->dbh;

    $dbh->do("DELETE FROM test_case_bugs 
               WHERE bug_id = ? 
                 AND case_run_id = ?", 
             undef, ($bug, $self->{'case_run_id'}));

}

=head2 update_bugs

Updates bug status depending on whether the case passed or failed. If
the case failed it will reopen any attached bugs that are closed. If it
passed it will mark RESOLVED bugs VERIFIED.

=cut

sub update_bugs {
    my $self = shift;
    my ($status) = @_;
    my $dbh = Bugzilla->dbh;
    my $timestamp = Bugzilla::Testopia::Util::get_time_stamp();
    foreach my $bug (@{$self->bugs}){
        my $oldstatus = $bug->bug_status;
        
        return if ($status eq 'VERIFIED' && $oldstatus ne 'RESOLVED');
        return if ($status eq 'REOPENED' && $oldstatus !~ /(RESOLVED|VERIFIED|CLOSED)/);
        
        my $comment  = "Status updated by Testopia:  ". Param('urlbase');
           $comment .= "tr_show_caserun.cgi?caserun_id=" . $self->id;
            
        $dbh->bz_lock_tables("bugs WRITE, fielddefs READ, longdescs WRITE, bugs_activity WRITE");
        $dbh->do("UPDATE bugs 
                     SET bug_status = ?,
                         delta_ts = ?
                     WHERE bug_id = ?", 
                     undef,($status, $timestamp, $bug->bug_id));
        LogActivityEntry($bug->bug_id, 'bug_status', $oldstatus, 
                         $status, Bugzilla->user->id, $timestamp);
        LogActivityEntry($bug->bug_id, 'resolution', $bug->resolution, '', 
                         Bugzilla->user->id, $timestamp) if ($status eq 'REOPENED');
        AppendComment($bug->bug_id, Bugzilla->user->id, $comment, 
                      !Bugzilla->user->in_group(Param('insidergroup')), $timestamp);
        
        $dbh->bz_unlock_tables();
    }
}

=head2 obliterate

Removes this caserun, its history, and all things that reference it.

=cut

sub obliterate {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    
    $dbh->do("DELETE FROM test_case_bugs WHERE case_run_id IN (" . 
              join(",", @{$self->get_case_run_list}) . ")", undef, $self->id)
                  if $self->get_case_run_list;
                  
    $dbh->do("DELETE FROM test_case_runs WHERE case_id = ? AND run_id = ?", 
              undef, ($self->case_id, $self->run_id));
    return 1;
}
   
###############################
####      Accessors        ####
###############################

=head1 ACCESSOR METHODS

=head2 id

Returns the ID of the object

=head2 testedby

Returns a Bugzilla::User object representing the user that closed
this case-run.

=head2 assignee

Returns a Bugzilla::User object representing the user assigned to 
update this case-run.

=head2 case_text_version

Returns the version of the test case document that this case-run
was run against.

=head2 notes

Returns the notes of the object

=head2 close_date

Returns the time stamp of when this case-run was closed

=head2 iscurrent

Returns true if this is the current case-run in the history list

=head2 status_id

Returns the status id of the object

=head2 sortkey

Returns the sortkey of the object

=head2 isprivate

Returns the true if this case-run is private.

=cut

=head2 updated_deps

Returns a reference to a list of dependent caseruns that were updated 

=cut

sub id                { return $_[0]->{'case_run_id'};          }
sub case_id           { return $_[0]->{'case_id'};          }
sub run_id            { return $_[0]->{'run_id'};          }
sub testedby          { return Bugzilla::User->new($_[0]->{'testedby'});   }
sub assignee          { return Bugzilla::User->new($_[0]->{'assignee'});   }
sub case_text_version { return $_[0]->{'case_text_version'};   }
sub close_date        { return $_[0]->{'close_date'};   }
sub iscurrent         { return $_[0]->{'iscurrent'};   }
sub status_id         { return $_[0]->{'case_run_status_id'};   }
sub sortkey           { return $_[0]->{'sortkey'};   }
sub isprivate         { return $_[0]->{'isprivate'};   }
sub updated_deps      { return $_[0]->{'updated_deps'};   }

=head2 type

Returns 'case'

=cut

sub type {
    my $self = shift;
    $self->{'type'} = 'caserun';
    return $self->{'type'};
}

=head2 notes

Returns the cumulative notes of all caserun records of this case and run.

=cut

sub notes { 
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my $notes = $dbh->selectcol_arrayref(
            "SELECT notes
               FROM test_case_runs
              WHERE case_id = ? AND run_id = ?
           ORDER BY case_run_id",
           undef,($self->case_id, $self->run_id));
    
    return join("\n", @$notes);
}

=head2 run

Returns the TestRun object that this case-run is associated with

=cut

# The potential exists for creating a circular reference here.
sub run {
    my $self = shift;
    return $self->{'run'} if exists $self->{'run'};
    $self->{'run'} = Bugzilla::Testopia::TestRun->new($self->{'run_id'});
    return $self->{'run'};
}

=head2 case

Returns the TestCase object that this case-run is associated with

=cut

# The potential exists for creating a circular reference here.
sub case {
    my $self = shift;
    return $self->{'case'} if exists $self->{'case'};
    $self->{'case'} = Bugzilla::Testopia::TestCase->new($self->{'case_id'});
    return $self->{'case'};
}

=head2 build

Returns the Build object that this case-run is associated with

=cut

sub build {
    my $self = shift;
    return $self->{'build'} if exists $self->{'build'};
    $self->{'build'} = Bugzilla::Testopia::Build->new($self->{'build_id'});
    return $self->{'build'};
}

=head2 environment

Returns the Build object that this case-run is associated with

=cut

sub environment {
    my $self = shift;
    return $self->{'environment'} if exists $self->{'environment'};
    $self->{'environment'} = Bugzilla::Testopia::Environment->new($self->{'environment_id'});
    return $self->{'environment'};
}

=head2 status

Looks up the status name of the associated status_id for this object

=cut

sub status {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    ($self->{'status'}) = $dbh->selectrow_array(
        "SELECT name FROM test_case_run_status
         WHERE case_run_status_id=?", undef,
         $self->{'case_run_status_id'});
    return $self->{'status'};
}

=head2 bugs

Returns a list of Bugzilla::Bug objects associated with this case-run 

=cut

sub bugs {
    my $self = shift;
    #return $self->{'bug'} if exists $self->{'bug'};
    my $dbh = Bugzilla->dbh;
    my @bugs;
    my $bugids = $dbh->selectcol_arrayref("SELECT bug_id 
                                     FROM test_case_bugs 
                                     WHERE case_run_id=?", 
                                     undef, $self->{'case_run_id'});
    foreach my $bugid (@{$bugids}){
        push @bugs, Bugzilla::Bug->new($bugid, Bugzilla->user->id) if Bugzilla->user->can_see_bug($bugid);
    }
    $self->{'bugs'} = \@bugs; #join(",", @$bugids);
    
    return $self->{'bugs'};
}

=head2 bug_list

Returns a comma separated list of bug ids associated with this case-run

=cut

sub bug_list {
    my $self = shift;
    return $self->{'bug_list'} if exists $self->{'bug_list'};
    my $dbh = Bugzilla->dbh;
    my @bugs;
    my $bugids = $dbh->selectcol_arrayref("SELECT bug_id 
                                     FROM test_case_bugs 
                                     WHERE case_run_id=?", 
                                     undef, $self->id);
    my @visible;
    foreach my $bugid (@{$bugids}){
        push @visible, $bugid if Bugzilla->user->can_see_bug($bugid);
    }
    $self->{'bug_list'} = join(",", @$bugids);
    
    return $self->{'bug_list'};
}

=head2 bug_count

Retuns a count of the bugs associated with this case-run

=cut

sub bug_count{
    my $self = shift;
    return $self->{'bug_count'} if exists $self->{'bug_count'};
    my $dbh = Bugzilla->dbh;

    $self->{'bug_count'} = $dbh->selectrow_array("SELECT COUNT(bug_id) 
                                                 FROM test_case_bugs 
                                                 WHERE case_run_id=?",
                                                 undef, $self->{'case_run_id'});
    return $self->{'bug_count'};
}

=head2 get_buglist

Returns a comma separated string off bug ids associated with 
this case-run

=cut

sub get_buglist {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my $bugids = $dbh->selectcol_arrayref("SELECT bug_id 
                                     FROM test_case_bugs 
                                     WHERE case_run_id=?", 
                                     undef, $self->{'case_run_id'});
    return join(',', @{$bugids});
}

=head2 is_open_status

Returns true if the status of this case-run is an open status

=cut

sub is_open_status {
    my $self = shift;
    my $status = shift;
    my @open_status_list = (IDLE, RUNNING, PAUSED);
    return 1 if lsearch(\@open_status_list, $status) > -1;
}

=head2 is_closed_status

Returns true if the status of this case-run is a closed status

=cut

sub is_closed_status {
    my $self = shift;
    my $status = shift;
    my @closed_status_list = (PASSED, FAILED, BLOCKED);
    return 1 if lsearch(\@closed_status_list, $status) > -1;
}

=head2 canview

Returns true if the logged in user has rights to view this case-run.

=cut

sub canview {
    my $self = shift;
    return 1 if Bugzilla->user->in_group('Testers');
    return 1 if $self->run->plan->get_user_rights(Bugzilla->user->id, GRANT_REGEXP) > 0;
    return 1 if $self->run->plan->get_user_rights(Bugzilla->user->id, GRANT_DIRECT) > 0;
    return 0;
}

=head2 canedit

Returns true if the logged in user has rights to edit this case-run.

=cut

sub canedit {
    my $self = shift;
    return 1 if Bugzilla->user->in_group('Testers');
    return 1 if $self->run->plan->get_user_rights(Bugzilla->user->id, GRANT_REGEXP) & 2;
    return 1 if $self->run->plan->get_user_rights(Bugzilla->user->id, GRANT_DIRECT) & 2;
    return 0;
}

=head2 candelete

Returns true if the logged in user has rights to delete this case-run.

=cut

sub candelete {
    my $self = shift;
    return 1 if Bugzilla->user->in_group('admin');
    return 0 unless Param("allow-test-deletion");
    return 1 if Bugzilla->user->in_group('Testers') && Param("testopia-allow-group-member-deletes");
    return 1 if $self->run->plan->get_user_rights(Bugzilla->user->id, GRANT_REGEXP) & 4;
    return 1 if $self->run->plan->get_user_rights(Bugzilla->user->id, GRANT_DIRECT) & 4;
    return 0;
}

=head1 SEE ALSO

TestCase TestRun

=head1 AUTHOR

Greg Hendricks <ghendricks@novell.com>

=cut

1;
