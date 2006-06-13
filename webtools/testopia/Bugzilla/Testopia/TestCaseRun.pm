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
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::Constants;

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
    notes
    close_date
    iscurrent
    sortkey

=cut

use constant DB_COLUMNS => qw(
    test_case_runs.case_run_id
    test_case_runs.run_id
    test_case_runs.case_id
    test_case_runs.assignee
    test_case_runs.testedby
    test_case_runs.case_run_status_id
    test_case_runs.case_text_version
    test_case_runs.build_id
    test_case_runs.notes
    test_case_runs.close_date
    test_case_runs.iscurrent
    test_case_runs.sortkey
);

our $columns = join(", ", DB_COLUMNS);


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
    my ($param) = (@_);
    my $dbh = Bugzilla->dbh;

    my $id = $param unless (ref $param eq 'HASH');
    my $obj;

    if (defined $id && detaint_natural($id)) {

        $obj = $dbh->selectrow_hashref(qq{
            SELECT $columns FROM test_case_runs
            WHERE case_run_id = ?}, undef, $id);

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
              VALUES (?,?,?,?,?,?,?,?,?,?,?,?)", undef,
              (undef, $self->{'run_id'}, $self->{'case_id'}, $self->{'assignee'},
               $self->{'testedby'}, IDLE, $self->{'case_text_version'}, 
               $self->{'build_id'}, $self->{'notes'}, undef, 1, 0));

    my $key = $dbh->bz_last_key( 'test_case_runs', 'case_run_id' );
    return $key;               
}

=head2 clone

Creates a copy of this caserun and sets it as the current record

=cut

sub clone {
    my $self = shift;
    my ($fields) = @_;
    my $dbh = Bugzilla->dbh;
    $self->{'build_id'} = $fields->{'build_id'};
    $self->{'case_run_status_id'} = IDLE;
    $self->{'notes'} = '';    
    my $entry = $self->store;
    $self->set_as_current($entry);
    return $entry;
}

=head2 update

Update this case-run in the database. This method checks which 
fields have been changed and either creates a clone of the case-run
or updates the existing one. 

=cut

sub update {
    my $self = shift;
    my ($fields) = @_;
    my $dbh = Bugzilla->dbh;
    if ($self->is_closed_status($fields->{'case_run_status_id'})){
        $fields->{'close_date'} = Bugzilla::Testopia::Util::get_time_stamp();
    }
    if ($fields->{'build_id'} != $self->{'build_id'} && $self->{'case_run_status_id'} != IDLE){
        return $self->clone($fields);
    }
    else {
        return $self->_update_fields($fields);
    }
    
}

=head2 _update_fields

Update this case-run in the database if a change is made to an 
updatable field.

=cut

sub _update_fields{
    my $self = shift;
    my ($newvalues) = @_;
    my $dbh = Bugzilla->dbh;

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

=head2 set_status

Sets the status on a case-run and updates the close_date and testedby 
if the status is a closed status.

=cut

sub set_status {
    my $self = shift;
    my ($status_id) = @_;
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
        $self->{'tested_by'} = Bugzilla->user;
    }
        
    $self->{'case_run_status_id'} = $status_id;
    $self->{'status'} = undef;
}

=head2 set_assignee

Sets the assigned tester for the case-run

=cut

sub set_assignee {
    my $self = shift;
    my ($user_id) = @_;
    $self->_update_fields({'assignee' => $user_id});
}

=head2 set_note

Updates the notes field for the case-run

=cut

sub set_note {
    my $self = shift;
    my ($note, $append) = @_;
    if ($append){
        my $note = $self->notes . "\n" . $note;
    }
    $self->_update_fields({'notes' => $note});
    $self->{'notes'} = $note;
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
    my ($bug) = @_;
    my $dbh = Bugzilla->dbh;

    $dbh->bz_lock_tables('test_case_bugs WRITE');
    my ($is) = $dbh->selectrow_array(
            "SELECT bug_id 
               FROM test_case_bugs 
              WHERE case_run_id=?
                AND bug_id=?", 
             undef, ($self->{'case_run_id'}, $bug));
    if ($is) {
        $dbh->bz_unlock_tables();
        return;
    }
    $dbh->do("INSERT INTO test_case_bugs (bug_id, case_run_id)
              VALUES(?,?)", undef, ($bug, $self->{'case_run_id'}));
    $dbh->bz_unlock_tables();
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

sub id                { return $_[0]->{'case_run_id'};          }
sub case_id           { return $_[0]->{'case_id'};          }
sub run_id            { return $_[0]->{'run_id'};          }
sub testedby          { return Bugzilla::User->new($_[0]->{'testedby'});   }
sub assignee          { return Bugzilla::User->new($_[0]->{'assignee'});   }
sub case_text_version { return $_[0]->{'case_text_version'};   }
sub notes             { return $_[0]->{'notes'};   }
sub close_date        { return $_[0]->{'close_date'};   }
sub iscurrent         { return $_[0]->{'iscurrent'};   }
sub status_id         { return $_[0]->{'case_run_status_id'};   }
sub sortkey           { return $_[0]->{'sortkey'};   }
sub isprivate         { return $_[0]->{'isprivate'};   }

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

=head2 run

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
    #TODO: Check that user can see bug                                     
    foreach my $bugid (@{$bugids}){
        push @bugs, Bugzilla::Bug->new($bugid, Bugzilla->user->id);
    }
    $self->{'bugs'} = \@bugs; #join(",", @$bugids);
    
    return $self->{'bugs'};
}

=head2 bug_list

Returns a comma separated list of bug ids associated with this case-run

=cut

sub bug_list {
    my $self = shift;
    #return $self->{'bug'} if exists $self->{'bug'};
    my $dbh = Bugzilla->dbh;
    my @bugs;
    my $bugids = $dbh->selectcol_arrayref("SELECT bug_id 
                                     FROM test_case_bugs 
                                     WHERE case_run_id=?", 
                                     undef, $self->{'case_run_id'});
    #TODO: Check that user can see bug                                     
    foreach my $bugid (@{$bugids}){
        push @bugs, Bugzilla::Bug->new($bugid, Bugzilla->user->id);
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

=head2 is_open_status

Returns true if the status of this case-run is an open status

=cut

sub is_open_status {
    my $self = shift;
    my $status = shift;
    my @open_status_list = (IDLE, RUNNING, PAUSED);
    return 1 if lsearch(\@open_status_list, $status) > 0;
}

=head2 is_open_status

Returns true if the status of this case-run is a closed status

=cut

sub is_closed_status {
    my $self = shift;
    my $status = shift;
    my @closed_status_list = (PASSED, FAILED, BLOCKED);
    return 1 if lsearch(\@closed_status_list, $status) > 0;
}

=head2 canview

Returns true if the logged in user has rights to view this case-run.

=cut

sub canview {

  my $self = shift;
#  return $self->{'canview'} if exists $self->{'canview'};
#  my ($case_log_id, $run_id, $plan_id, $current_user_id) = @_;
#
#  my $dbh = Bugzilla->dbh;
#  my $canview = 0;
#  my $current_user_id = Bugzilla->user->id;
#  my ($plan_id) = $dbh->selectrow_array("SELECT plan_id FROM test_runs 
#                                         WHERE run_id=?", 
#                                         undef, $self->{'test_run_id'}); 
#  
#  if (0 == &Bugzilla::Param('private-cases-log')) {
#    $canview = 1;
#  } else {    
#        
#    if (0 == $self->{'isprivate'}) {
#      # if !isprivate, then everybody can run it and should be able to see
#      # the current status
#      $canview = 1;
#    } else {      
#      # check is the current user is a tester:
#      if (defined $current_user_id) {
#
#        SendSQL("select 1 from test_case_run_testers ".
#                "where case_log_id=". $self->{'id'} ." and userid=$current_user_id");
#                 
#        if (FetchOneColumn()) {
#          # current user is a tester
#          $canview = 1;
#        } else {
#            # check editors
#            SendSQL("select 1 from test_plans where plan_id=$plan_id and editor=$current_user_id");
#            
#            if (FetchOneColumn()) {
#              $canview = 1;
#            } else {
#              # check watchers
#              SendSQL("select 1 from test_plans_watchers where plan_id=$plan_id and userid=$current_user_id");
#              if (FetchOneColumn()) {
#                $canview = 1;
#              } else {
#                #check test run manager
#                SendSQL("select 1 from test_runs where test_run_id=". $self->{'test_run_id'} ." and manager=$current_user_id");
#                $canview = FetchOneColumn()? 1 : 0;
#                
#                if (0 == $canview) {
#                  if (UserInGroup('admin')) {
#                    $canview = 1;
#                  }
#                }
#              }
#            }
#        }
#      }      
#    }    
#  }
  
  $self->{'canview'} = 1;
  return $self->{'canview'};
}

=head2 canedit

Returns true if the logged in user has rights to edit this case-run.

=cut

sub canedit {
    my $self = shift;
    return $self->canview 
    && (UserInGroup('managetestplans') 
      || UserInGroup('edittestcases')
        || UserInGroup('runtests'));
}

=head2 canedit

Returns true if the logged in user has rights to delete this case-run.

=cut

sub candelete {
    my $self = shift;
    return $self->canedit && Param('allow-test-deletion');
}

=head1 SEE ALSO

TestCase TestRun

=head1 AUTHOR

Greg Hendricks <ghendricks@novell.com>

=cut

1;
