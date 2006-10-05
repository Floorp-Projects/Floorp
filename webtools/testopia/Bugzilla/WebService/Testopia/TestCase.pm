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

package Bugzilla::WebService::Testopia::TestCase;

use strict;
use base qw(Bugzilla::WebService);
use Bugzilla::User;
use Bugzilla::Util qw(detaint_natural);
use Bugzilla::Testopia::TestCase;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;


# Convert string field values to their respective integer id's
sub _convert_to_ids
{
    my ($hash) = @_;

    if (defined($$hash{"author"}))
    {
    	$$hash{"author_id"} = login_to_id($$hash{"author"});
    }
   	delete $$hash{"author"};
 
    if (defined($$hash{"case_status"}))
    {
    	$$hash{"case_status_id"} = Bugzilla::Testopia::TestCase::lookup_status_by_name($$hash{"case_status"});
    }
   	delete $$hash{"case_status"};
 
    if (defined($$hash{"category"}))
    {
    	$$hash{"category_id"} = Bugzilla::Testopia::TestCase::lookup_category_by_name($$hash{"category"});
    }
   	delete $$hash{"category"};
 
    if (defined($$hash{"default_tester"}))
    {
    	$$hash{"default_tester_id"} = login_to_id($$hash{"default_tester"});
    }
   	delete $$hash{"default_tester"};
 
    if (defined($$hash{"priority"}))
    {
    	$$hash{"priority_id"} = Bugzilla::Testopia::TestCase::lookup_priority_by_value($$hash{"priority"});
    }
   	delete $$hash{"priority"};
 }

# Convert fields with integer id's to their respective string values
sub _convert_to_strings
{
	my ($hash) = @_;

 	$$hash{"author"} = "";
    if (defined($$hash{"author_id"}))
    {
    	$$hash{"author"} = new Bugzilla::User($$hash{"author_id"})->login;
    }
   	delete $$hash{"author_id"};
 
 	$$hash{"case_status"} = "";
    if (defined($$hash{"case_status_id"}))
    {
    	$$hash{"case_status"} = Bugzilla::Testopia::TestCase->lookup_status($$hash{"case_status_id"});
    }
   	delete $$hash{"case_status_id"};
 
 	$$hash{"category"} = "";
    if (defined($$hash{"category_id"}))
    {
    	$$hash{"category"} = Bugzilla::Testopia::TestCase->lookup_category($$hash{"category_id"});
    }
   	delete $$hash{"category_id"};
 
 	$$hash{"default_tester"} = "";
    if (defined($$hash{"default_tester_id"}))
    {
    	$$hash{"default_tester"} = new Bugzilla::User($$hash{"default_tester_id"})->login;
    }  
   	delete $$hash{"default_tester_id"};
 
 	$$hash{"priority"} = "";
    if (defined($$hash{"priority_id"}))
    {
    	$$hash{"priority"} = Bugzilla::Testopia::TestCase->lookup_priority($$hash{"priority_id"});
    }
   	delete $$hash{"priority_id"};
 }

# Utility method called by the list method
sub _list
{
    my ($query) = @_;
    
    my $cgi = Bugzilla->cgi;

    $cgi->param("viewall", 1);

    $cgi->param("current_tab", "case");
    
    foreach (keys(%$query))
    {
    	$cgi->param($_, $$query{$_});
    }
    	
    my $search = Bugzilla::Testopia::Search->new($cgi);

	# Result is an array of test plan hash maps 
	return Bugzilla::Testopia::Table->new('case', 
	                                      'tr_xmlrpc.cgi', 
	                                      $cgi, 
	                                      undef,
	                                      $search->query()
	                                      )->list();
}

sub get
{
    my $self = shift;
    my ($test_case_id) = @_;

    Bugzilla->login;

    # We can detaint immediately if what we get passed is fully numeric.
    # We leave bug alias checks to Bugzilla::Testopia::TestCase::new.
    
    if ($test_case_id =~ /^[0-9]+$/) {
        detaint_natural($test_case_id);
    }

	#Result is a test case hash map
    my $testcase = new Bugzilla::Testopia::TestCase($test_case_id);
    
    _convert_to_strings($testcase);
    
    Bugzilla->logout;
    
    return $testcase;
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
	
	my $test_case = new Bugzilla::Testopia::TestCase($new_values);
	
	Bugzilla->logout;
	
	# Result is new test plan id
	return $test_case->store();
}

sub update
{
	my $self =shift;
	my ($test_case_id, $new_values) = @_;

    Bugzilla->login;

	my $test_case = new Bugzilla::Testopia::TestCase($test_case_id);

    _convert_to_ids($new_values);
	
	$test_case->update($new_values);
	
	Bugzilla->logout;
	
	# Result is zero on success, otherwise an exception will be thrown
	return 0;
}

sub get_text
{
	my $self =shift;
	my ($test_case_id) = @_;

    Bugzilla->login;
    
	my $test_case = new Bugzilla::Testopia::TestCase($test_case_id);

	my $doc = $test_case->text();

 	$$doc{"author"} = "";
    if (defined($$doc{"author_id"}))
    {
    	$$doc{"author"} = new Bugzilla::User($$doc{"author_id"})->login;
    }  
   	delete $$doc{"author_id"};
	
	Bugzilla->logout;
	
	#Result is the latest test case doc hash map
	return $doc;
}

sub store_text
{
	my $self =shift;
	my ($test_case_id, $author, $action, $effect) = @_;

    Bugzilla->login;
    
	my $author_id = login_to_id($author);
	
	my $test_case = new Bugzilla::Testopia::TestCase($test_case_id);

	my $version = $test_case->store_text($test_case_id, $author_id, $action, $effect);
	
	Bugzilla->logout;
	
	# Result is new test case doc version on success, otherwise an exception will be thrown
	return $version;
}

1;