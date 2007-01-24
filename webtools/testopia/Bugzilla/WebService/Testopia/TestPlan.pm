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

package Bugzilla::WebService::Testopia::TestPlan;

use strict;

use base qw(Bugzilla::WebService);

use Bugzilla::Constants;
use Bugzilla::User;
use Bugzilla::Testopia::TestPlan;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;

# Utility method called by the list method
sub _list
{
    my ($query) = @_;
    
    my $cgi = Bugzilla->cgi;

    $cgi->param("current_tab", "plan");
    
    foreach (keys(%$query))
    {
    	$cgi->param($_, $$query{$_});
    }
    	
    my $search = Bugzilla::Testopia::Search->new($cgi);

	# Result is an array of test plan hash maps 
	return Bugzilla::Testopia::Table->new('plan', 
	                                      'tr_xmlrpc.cgi', 
	                                      $cgi, 
	                                      undef,
	                                      $search->query()
	                                      )->list();
}

sub get
{
    my $self = shift;
    my ($test_plan_id) = @_;

    $self->login;    

	#Result is a test plan hash map
    my $test_plan = new Bugzilla::Testopia::TestPlan($test_plan_id);

	if (not defined $test_plan)
	{
    	$self->logout;
        die "Testplan, " . $test_plan_id . ", not found"; 
	}
	
	if (not $test_plan->canview)
	{
	    $self->logout;
        die "User Not Authorized";
	}
    
    $self->logout;

    return $test_plan;
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

	my $test_plan = new Bugzilla::Testopia::TestPlan($new_values);
	
	my $result = $test_plan->store(); 
	
	$self->logout;
	
	# Result is new test plan id
	return $result;
}

sub update
{
	my $self =shift;
	my ($test_plan_id, $new_values) = @_;

    $self->login;

	my $test_plan = new Bugzilla::Testopia::TestPlan($test_plan_id);
	
	if (not defined $test_plan)
	{
    	$self->logout;
        die "Testplan, " . $test_plan_id . ", not found"; 
	}
	
	if (not $test_plan->canedit)
	{
	    $self->logout;
        die "User Not Authorized";
	}

    my $result = $test_plan->update($new_values);

	$test_plan = new Bugzilla::Testopia::TestPlan($test_plan_id);
	
	$self->logout;

	# Result is modified test plan, otherwise an exception will be thrown
	return $test_plan;
}

sub get_test_cases
{
	my $self =shift;
    my ($test_plan_id) = @_;

    $self->login;

    my $test_plan = new Bugzilla::Testopia::TestPlan($test_plan_id);

	if (not defined $test_plan)
	{
    	$self->logout;
        die "Testplan, " . $test_plan_id . ", not found"; 
	}
	
	if (not $test_plan->canview)
	{
	    $self->logout;
        die "User Not Authorized";
	}
    
    my $result = $test_plan->test_cases();
	
	$self->logout;

	# Result is list of test cases for the given test plan
	return $result;
}

sub get_test_runs
{
	my $self =shift;
    my ($test_plan_id) = @_;

    $self->login;

    my $test_plan = new Bugzilla::Testopia::TestPlan($test_plan_id);

	if (not defined $test_plan)
	{
    	$self->logout;
        die "Testplan, " . $test_plan_id . ", not found"; 
	}
	
	if (not $test_plan->canview)
	{
	    $self->logout;
        die "User Not Authorized";
	}
    
    my $result = $test_plan->test_runs();

	$self->logout;
	
	# Result is list of test runs for the given test plan
	return $result;
}

sub get_categories
{
	my $self =shift;
    my ($test_plan_id) = @_;

    $self->login;

    my $test_plan = new Bugzilla::Testopia::TestPlan($test_plan_id);

	if (not defined $test_plan)
	{
    	$self->logout;
        die "Testplan, " . $test_plan_id . ", not found"; 
	}
	
	if (not $test_plan->canview)
	{
	    $self->logout;
        die "User Not Authorized";
	}
    
    my $result = $test_plan->product->categories();

	$self->logout;
	
	# Result is list of categories for the given test plan
	return $result;
}

sub get_components
{
	my $self =shift;
    my ($test_plan_id) = @_;

    $self->login;

    my $test_plan = new Bugzilla::Testopia::TestPlan($test_plan_id);

	if (not defined $test_plan)
	{
    	$self->logout;
        die "Testplan, " . $test_plan_id . ", not found"; 
	}
	
	if (not $test_plan->canview)
	{
	    $self->logout;
        die "User Not Authorized";
	}
    
    my $result = $test_plan->product->components;

	$self->logout;
	
	# Result is list of components for the given test plan
	return $result;
}

sub get_builds
{
	my $self =shift;
    my ($test_plan_id) = @_;

    $self->login;

    my $test_plan = new Bugzilla::Testopia::TestPlan($test_plan_id);

	if (not defined $test_plan)
	{
    	$self->logout;
        die "Testplan, " . $test_plan_id . ", not found"; 
	}
	
	if (not $test_plan->canview)
	{
	    $self->logout;
        die "User Not Authorized";
	}
    
    my $result = $test_plan->product->builds();

	$self->logout;
	
	# Result is list of builds for the given test plan
	return $result;
}

sub lookup_type_name_by_id
{
	my $self =shift;
    my ($id) = @_;
    
    $self->login;

    my $test_plan = new Bugzilla::Testopia::TestPlan({});
    
    my $result = $test_plan->lookup_type($id);

	$self->logout;
	
	# Result is test plan type name for the given test plan type id
	return $result;
}

sub lookup_type_id_by_name
{
	my $self =shift;
    my ($name) = @_;
    
    $self->login;

    my $test_plan = new Bugzilla::Testopia::TestPlan({});
    
    my $result = $test_plan->lookup_type_by_name($name);

	$self->logout;

    if (!defined $result) 
    {
      $result = 0;
    };
	
	# Result is test plan type id for the given test plan type name
	return $result;
}

sub add_tag
{
	my $self =shift;
	my ($test_plan_id, $tag_name) = @_;

    $self->login;

	my $test_plan = new Bugzilla::Testopia::TestPlan($test_plan_id);

	if (not defined $test_plan)
	{
    	$self->logout;
        die "Testplan, " . $test_plan_id . ", not found"; 
	}
	
	if (not $test_plan->canedit)
	{
	    $self->logout;
        die "User Not Authorized";
	}
	
	#Create new tag or retrieve id of existing tag
	my $test_tag = new Bugzilla::Testopia::TestTag({tag_name=>$tag_name});
	my $tag_id = $test_tag->store;

    my $result = $test_plan->add_tag($tag_id);
    
    if ($result == 1)
    {
        $self->logout;
        die "Tag, " . $tag_name . ", already exists for Testplan, " . $test_plan_id;
    }

	$self->logout;
	
	# Result 0 on success, otherwise an exception will be thrown
	return $result;
}

sub remove_tag
{
	my $self =shift;
	my ($test_plan_id, $tag_name) = @_;

    $self->login;

	my $test_plan = new Bugzilla::Testopia::TestPlan($test_plan_id);

	if (not defined $test_plan)
	{
    	$self->logout;
        die "Testplan, " . $test_plan_id . ", not found"; 
	}
	
	if (not $test_plan->canedit)
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
    
    my $result = $test_plan->remove_tag($test_tag->id);

	$self->logout;
	
	# Result 0 on success, otherwise an exception will be thrown
	return 0;
}

sub get_tags
{
	my $self =shift;
	my ($test_plan_id) = @_;

    $self->login;

	my $test_plan = new Bugzilla::Testopia::TestPlan($test_plan_id);

	if (not defined $test_plan)
	{
    	$self->logout;
        die "Testplan, " . $test_plan_id . ", not found"; 
	}
	
	if (not $test_plan->canview)
	{
	    $self->logout;
        die "User Not Authorized";
	}

    my $result = $test_plan->tags;

	$self->logout;
	
	# Result list of tags otherwise an exception will be thrown
	return $result;
}

1;