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
use Bugzilla::Util qw(detaint_natural);
use Bugzilla::Testopia::TestCaseRun;
use Bugzilla::User;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;

# Convert string field values to their respective integer id's
sub _convert_to_ids
{
    my ($hash) = @_;

    if (defined($$hash{"assignee"}))
    {
    	$$hash{"assignee"} = login_to_id($$hash{"assignee"});
    }

    if (defined($$hash{"testedby"}))
    {
    	$$hash{"testedby"} = login_to_id($$hash{"testedby"});
    }

    if (defined($$hash{"case_run_status"}))
    {
    	$$hash{"case_run_status_id"} = Bugzilla::Testopia::TestCaseRun::lookup_status_by_name($$hash{"case_run_status"});
    }
   	delete $$hash{"case_run_status"};
}

# Convert fields with integer id's to their respective string values
sub _convert_to_strings
{
	my ($hash) = @_;

    if (defined($$hash{"assignee"}))
    {
    	$$hash{"assignee"} = new Bugzilla::User($$hash{"assignee"})->login;
    }

    if (defined($$hash{"testedby"}))
    {
    	$$hash{"testedby"} = new Bugzilla::User($$hash{"testedby"})->login;
    }

	$$hash{"case_run_status"} = "";    
    if (defined($$hash{"case_run_status_id"}))
    {
    	$$hash{"case_run_status"} = Bugzilla::Testopia::TestCaseRun->lookup_status($$hash{"case_run_status_id"});
    }
   	delete $$hash{"case_run_status_id"};
}

# Utility method called by the list method
sub _list
{
    my ($query) = @_;
    
    my $cgi = Bugzilla->cgi;

    $cgi->param("viewall", 1);

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

    Bugzilla->login;

    # We can detaint immediately if what we get passed is fully numeric.
    # We leave bug alias checks to Bugzilla::Testopia::TestCaseRun::new.
    
    if ($test_case_run_id =~ /^[0-9]+$/) {
        detaint_natural($test_case_run_id);
    }

    #Result is a test case run hash map
    my $test_case_run = new Bugzilla::Testopia::TestCaseRun($test_case_run_id);
    
    _convert_to_strings($test_case_run);
    
    Bugzilla->logout;
    
    return $test_case_run;
    
}

sub list
{
    Bugzilla->login;
    
    my $self = shift;
    my ($query) = @_;
    
    _convert_to_ids($query);
    
	my $list = _list($query);
	
	foreach (@$list)
	{
		_convert_to_strings($_);
	}
	    
    Bugzilla->logout;
    
    return $list;	
}

sub create
{
	my $self =shift;
	my ($new_values) = @_;

    Bugzilla->login;

    _convert_to_ids($new_values);
	
	my $test_case_run = new Bugzilla::Testopia::TestCaseRun($new_values);
	
	Bugzilla->logout;
	
	# Result is new test case run id
	return $test_case_run->store();
}

sub update
{
	my $self =shift;
	my ($run_id, $case_id, $build_id, $new_values) = @_;

    Bugzilla->login;

	my $test_case_run_id = Bugzilla::Testopia::TestCaseRun::lookup_case_run_id($run_id, $case_id, $build_id);
	
	my $test_case_run = new Bugzilla::Testopia::TestCaseRun($test_case_run_id);

    _convert_to_ids($new_values);
    
    $$new_values{build_id} = $build_id;  # Needed by TestCaseRun's update member function
	
	$test_case_run->update($new_values);
	
	Bugzilla->logout;
	
	# Result is zero on success, otherwise an exception will be thrown
	return 0;
}

1;
