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

use Bugzilla::Util qw(detaint_natural);
use Bugzilla::Product;
use Bugzilla::User;
use Bugzilla::Testopia::TestRun;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;

# Convert string field values to their respective integer id's
sub _convert_to_ids
{
    my ($hash) = @_;

    if (defined($$hash{"manager"}))
    {
    	$$hash{"manager_id"} = login_to_id($$hash{"manager"});
    }
   	delete $$hash{"manager"};

    if (defined($$hash{"environment"}))
    {
    	$$hash{"environment_id"} = Bugzilla::Testopia::TestRun::lookup_environment_by_name($$hash{"environment"});
    }
	delete $$hash{"environment"};
}

# Convert fields with integer id's to their respective string values
sub _convert_to_strings
{
	my ($hash) = @_;

	$$hash{"manager"} = ""; 
    if (defined($$hash{"manager_id"}))
    {
    	$$hash{"manager"} = new Bugzilla::User($$hash{"manager_id"})->login;
    }
   	delete $$hash{"manager_id"};

	$$hash{"environment"} = ""; 
    if (defined($$hash{"environment_id"}))
    {
    	$$hash{"environment"} = Bugzilla::Testopia::TestRun->lookup_environment($$hash{"environment_id"});
    }
   	delete $$hash{"environment_id"};
}

# Utility method called by the list method
sub _list
{
    my ($query) = @_;
    
    my $cgi = Bugzilla->cgi;

    $cgi->param("viewall", 1);

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

    Bugzilla->login;

    # We can detaint immediately if what we get passed is fully numeric.
    # We leave bug alias checks to Bugzilla::Testopia::TestRun::new.
    
    if ($test_run_id =~ /^[0-9]+$/) {
        detaint_natural($test_run_id);
    }

	#Result is a test run hash map
    my $testrun = new Bugzilla::Testopia::TestRun($test_run_id);
    
    _convert_to_strings($testrun);
    
    Bugzilla->logout;
    
    return $testrun;
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
	
	my $test_run = new Bugzilla::Testopia::TestRun($new_values);
	
	Bugzilla->logout;
	
	# Result is new test run id
	return $test_run->store();
}

sub update
{
	my $self =shift;
	my ($test_run_id, $new_values) = @_;

    Bugzilla->login;

	my $test_run = new Bugzilla::Testopia::TestRun($test_run_id);

    _convert_to_ids($new_values);
	
	$test_run->update($new_values);
	
	Bugzilla->logout;
	
	# Result is zero on success, otherwise an exception will be thrown
	return 0;
}

1;