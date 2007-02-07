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
# Contributor(s): Marc Schumann <wurblzap@gmail.com>
#                 Dallas Harken <dharken@novell.com>

package Bugzilla::WebService::Testopia::TestCaseRun;

use strict;

use base qw(Bugzilla::WebService);

use Bugzilla::User;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;
use Bugzilla::Testopia::TestCaseRun;

# Utility method called by the list method
sub _list
{
    my ($query) = @_;
    
    my $cgi = Bugzilla->cgi;

    $cgi->param("current_tab", "case_run");
    
    foreach (keys(%$query))
    {
    	$cgi->param($_, $$query{$_});
    }
    	
    my $search = Bugzilla::Testopia::Search->new($cgi);

	# Result is an array of test case run hash maps 
	return Bugzilla::Testopia::Table->new('case_run', 
	                                      'tr_xmlrpc.cgi', 
	                                      $cgi, 
	                                      undef,
	                                      $search->query()
	                                      )->list();
}

sub get
{
    my $self = shift;
    my ($test_case_run_id) = @_;

    $self->login;

    #Result is a test case run hash map
    my $test_case_run = new Bugzilla::Testopia::TestCaseRun($test_case_run_id);

	if (not defined $test_case_run)
	{
    	$self->logout;
        die "TestcaseRun, " . $test_case_run_id . ", not found"; 
	}
	
	if (not $test_case_run->canview)
	{
	    $self->logout;
        die "User Not Authorized";
	}
    
    $self->logout;
    
    return $test_case_run;
    
}

sub list
{
    my $self = shift;
    my ($query) = @_;

    $self->login;
    
	my $list = _list($query);
	
    $self->logout;
    
    return $list;	
}

sub create
{
	my $self =shift;
	my ($new_values) = @_;

    $self->login;

	my $test_case_run = new Bugzilla::Testopia::TestCaseRun($new_values);
	
	my $result = $test_case_run->store();
	
	$self->logout;

	# Result is new test case run id
	return $result;
}

sub update
{
	my $self =shift;
	my ($run_id, $case_id, $build_id, $environment_id, $new_values) = @_;

    $self->login;

    my $test_case_run = new Bugzilla::Testopia::TestCaseRun($case_id,
                                                            $run_id,
                                                            $build_id,
                                                            $environment_id
                                                            );

	if (not defined $test_case_run)
	{
    	$self->logout;
        die "TestCaseRun for build_id = " . $build_id .
            ", case_id = " . $case_id . 
            ", environment_id = " . $environment_id . 
            ", run_id = " . $run_id . 
            ", not found"; 
	}

	if (not $test_case_run->canedit)
	{
	    $self->logout;
        die "User Not Authorized";
	}

    # Check to see what has changed then use set methods
    # The order is important. We need to check if the build or environment has 
    # Changed so that we can switch to the right record first.
    if (defined($$new_values{build_id}))
    {
        $test_case_run = $test_case_run->switch($newvalues->{'build_id'}, $environment_id ,$run_id, $case_id);
    }

    if (defined($$new_values{environment_id}))
    {
        $test_case_run = $test_case_run->switch($build_id, $newvalues->{'environment_id'} ,$run_id, $case_id);
    }

    if (defined($$new_values{assignee}))
    {
        $test_case_run->set_assignee($$new_values{assignee});
    }

    if (defined($$new_values{case_run_status_id}))
    {
        $test_case_run->set_status($$new_values{case_run_status_id});
    }

    if (defined($$new_values{notes}))
    {
        $test_case_run->append_note($$new_values{notes});
    }
    
    # Remove assignee user object and replace with just assignee id
    if (ref($$test_case_run{assignee}) eq 'Bugzilla::User')
    {
        $$test_case_run{assignee} = $$test_case_run{assignee}->id();
    }

	$self->logout;

	#Remove attributes we do not want to publish
	delete $$test_case_run{bugs};
	delete $$test_case_run{build};
	delete $$test_case_run{case};
	delete $$test_case_run{environment};
	delete $$test_case_run{run};
	
	# Result is modified test case run on success, otherwise an exception will be thrown
	return $test_case_run;
}

sub lookup_status_id_by_name
{
	my $self =shift;
    my ($name) = @_;
    
    $self->login;

  	my $result = Bugzilla::Testopia::TestCaseRun::lookup_status_by_name($name);

	$self->logout;

	# Result is test case run status id for the given test case run status name
	return $result;
}

sub lookup_status_name_by_id
{
	my $self =shift;
    my ($id) = @_;
    
    $self->login;

    my $test_case_run = new Bugzilla::Testopia::TestCaseRun({});
     
    my $result = $test_case_run->lookup_status($id);

	$self->logout;

    if (!defined $result) 
    {
      $result = 0;
    };
	
	# Result is test case run status name for the given test case run status id
	return $result;
}

1;
