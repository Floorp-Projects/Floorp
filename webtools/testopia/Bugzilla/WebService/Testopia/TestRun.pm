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

package Bugzilla::WebService::Testopia::TestRun;

use strict;

use base qw(Bugzilla::WebService);

use Bugzilla::Product;
use Bugzilla::User;
use Bugzilla::Testopia::TestRun;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;

# Utility method called by the list method
sub _list
{
    my ($query) = @_;
    
    my $cgi = Bugzilla->cgi;

    $cgi->param("current_tab", "run");
    
    foreach (keys(%$query))
    {
    	$cgi->param($_, $$query{$_});
    }
    	
    my $search = Bugzilla::Testopia::Search->new($cgi);

	# Result is an array of test run hash maps 
	return Bugzilla::Testopia::Table->new('run', 
	                                      'tr_xmlrpc.cgi', 
	                                      $cgi, 
	                                      undef,
	                                      $search->query()
	                                      )->list();
}

sub get
{
    my $self = shift;
    my ($test_run_id) = @_;

    $self->login;

	#Result is a test run hash map
    my $test_run = new Bugzilla::Testopia::TestRun($test_run_id);

	if (not defined $test_run)
	{
    	$self->logout;
        die "Testrun, " . $test_run_id . ", not found"; 
	}
	
	if (not $test_run->canview)
	{
	    $self->logout;
        die "User Not Authorized";
	}
    
    $self->logout;
    
    return $test_run;
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

	my $test_run = new Bugzilla::Testopia::TestRun($new_values);
	
	my $result = $test_run->store();
	
	$self->logout;
	
	# Result is new test run id
	return $result
}

sub update
{
	my $self =shift;
	my ($test_run_id, $new_values) = @_;

    $self->login;

	my $test_run = new Bugzilla::Testopia::TestRun($test_run_id);

	if (not defined $test_run)
	{
    	$self->logout;
        die "Testrun, " . $test_run_id . ", not found"; 
	}
	
	if (not $test_run->canedit)
	{
	    $self->logout;
        die "User Not Authorized";
	}

	my $result = $test_run->update($new_values);

	$test_run = new Bugzilla::Testopia::TestRun($test_run_id);
	
	$self->logout;
	
	# Result is modified test run on success, otherwise an exception will be thrown
	return $test_run;
}

sub get_test_cases
{
	my $self =shift;
    my ($test_run_id) = @_;

    $self->login;

    my $test_run = new Bugzilla::Testopia::TestRun($test_run_id);

	if (not defined $test_run)
	{
    	$self->logout;
        die "Testrun, " . $test_run_id . ", not found"; 
	}
	
	if (not $test_run->canview)
	{
	    $self->logout;
        die "User Not Authorized";
	}
    
    my $result = $test_run->cases();

	$self->logout;
	
	# Result is list of test cases for the given test run
	return $result;
}

sub get_test_case_runs
{
	my $self =shift;
    my ($test_run_id) = @_;

    $self->login;

    my $test_run = new Bugzilla::Testopia::TestRun($test_run_id);

	if (not defined $test_run)
	{
    	$self->logout;
        die "Testrun, " . $test_run_id . ", not found"; 
	}
	
	if (not $test_run->canview)
	{
	    $self->logout;
        die "User Not Authorized";
	}
    
    my $result = $test_run->caseruns();

	$self->logout;
	
	# Result is list of test case runs for the given test run
	return $result;
}

sub get_test_plan
{
	my $self =shift;
    my ($test_run_id) = @_;

    $self->login;

    my $test_run = new Bugzilla::Testopia::TestRun($test_run_id);

	if (not defined $test_run)
	{
    	$self->logout;
        die "Testrun, " . $test_run_id . ", not found"; 
	}
	
	if (not $test_run->canview)
	{
	    $self->logout;
        die "User Not Authorized";
	}
    
    my $result = $test_run->plan();

	$self->logout;
	
	# Result is test plan for the given test run
	return $result;
}

sub lookup_environment_id_by_name
{
	my $self =shift;
    my ($name) = @_;
    
    $self->login;

  	my $result = Bugzilla::Testopia::TestRun::lookup_environment_by_name($name);

	$self->logout;

	# Result is test run environment id for the given test run environment name
	return $result;
}

sub lookup_environment_name_by_id
{
	my $self =shift;
    my ($id) = @_;
    
    $self->login;

    my $test_run = new Bugzilla::Testopia::TestRun({});
     
    my $result = $test_run->lookup_environment($id);

	$self->logout;

    if (!defined $result) 
    {
      $result = 0;
    };
	
	# Result is test run environment name for the given test run environment id
	return $result;
}

sub add_tag
{
	my $self =shift;
	my ($test_run_id, $tag_name) = @_;

    $self->login;

	my $test_run = new Bugzilla::Testopia::TestRun($test_run_id);

	if (not defined $test_run)
	{
    	$self->logout;
        die "Testrun, " . $test_run_id . ", not found"; 
	}
	
	if (not $test_run->canedit)
	{
	    $self->logout;
        die "User Not Authorized";
	}
	
	#Create new tag or retrieve id of existing tag
	my $test_tag = new Bugzilla::Testopia::TestTag({tag_name=>$tag_name});
	my $tag_id = $test_tag->store;

    my $result = $test_run->add_tag($tag_id);
    
    if ($result == 1)
    {
        $self->logout;
        die "Tag, " . $tag_name . ", already exists for Testrun, " . $test_run_id;
    }

	$self->logout;
	
	# Result 0 on success, otherwise an exception will be thrown
	return $result;
}

sub remove_tag
{
	my $self =shift;
	my ($test_run_id, $tag_name) = @_;

    $self->login;

	my $test_run = new Bugzilla::Testopia::TestRun($test_run_id);

	if (not defined $test_run)
	{
    	$self->logout;
        die "Testrun, " . $test_run_id . ", not found"; 
	}
	
	if (not $test_run->canedit)
	{
	    $self->logout;
        die "User Not Authorized";
	}

    my $test_tag = Bugzilla::Testopia::TestTag->check_name($tag_name);
    if (not defined $test_tag)
    {
        $self->logout;
        die "Tag, " . $tag_name . ", does not exist";
    }
    
    my $result = $test_run->remove_tag($test_tag->id);

	$self->logout;
	
	# Result 0 on success, otherwise an exception will be thrown
	return 0;
}

sub get_tags
{
	my $self =shift;
	my ($test_run_id) = @_;

    $self->login;

	my $test_run = new Bugzilla::Testopia::TestRun($test_run_id);

	if (not defined $test_run)
	{
    	$self->logout;
        die "Testrun, " . $test_run_id . ", not found"; 
	}
	
	if (not $test_run->canview)
	{
	    $self->logout;
        die "User Not Authorized";
	}

    my $result = $test_run->tags;

	$self->logout;
	
	# Result list of tags otherwise an exception will be thrown
	return $result;
}

1;