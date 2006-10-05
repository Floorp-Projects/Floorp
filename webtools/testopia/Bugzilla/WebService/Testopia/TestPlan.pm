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

use Bugzilla::Util qw(detaint_natural);
use Bugzilla::Product;
use Bugzilla::User;
use Bugzilla::Testopia::TestPlan;
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
    if (defined($$hash{"product"}))
    {
    	my $product = Bugzilla::Product::check_product($$hash{"product"});
    	$$hash{"product_id"} = $product->id;
    }
   	delete $$hash{"product"};

    if (defined($$hash{"type"}))
    {
    	$$hash{"type_id"} = Bugzilla::Testopia::TestPlan::lookup_type_by_name($$hash{"type"});
    }
    delete $$hash{"type"};
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

	$$hash{"product"} = ""; 
    if (defined($$hash{"product_id"}))
    {
    	$$hash{"product"} = new Bugzilla::Product($$hash{"product_id"})->name;
    }
   	delete $$hash{"product_id"};

	$$hash{"type"} = ""; 
    if (defined($$hash{"type_id"}))
    {
    	$$hash{"type"} = Bugzilla::Testopia::TestPlan->lookup_type($$hash{"type_id"});
    }
   	delete $$hash{"type_id"};
}

# Utility method called by the list method
sub _list
{
    my ($query) = @_;
    
    my $cgi = Bugzilla->cgi;

    $cgi->param("viewall", 1);

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

    Bugzilla->login;

    # We can detaint immediately if what we get passed is fully numeric.
    # We leave bug alias checks to Bugzilla::Testopia::TestPlan::new.
    
    if ($test_plan_id =~ /^[0-9]+$/) {
        detaint_natural($test_plan_id);
    }

	#Result is a test plan hash map
    my $testplan = new Bugzilla::Testopia::TestPlan($test_plan_id);
    
    _convert_to_strings($testplan);
    
    Bugzilla->logout;
    
    return $testplan;
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
	
	my $test_plan = new Bugzilla::Testopia::TestPlan($new_values);
	
	Bugzilla->logout;
	
	# Result is new test plan id
	return $test_plan->store();
}

sub update
{
	my $self =shift;
	my ($test_plan_id, $new_values) = @_;

    Bugzilla->login;

	my $test_plan = new Bugzilla::Testopia::TestPlan($test_plan_id);

    _convert_to_ids($new_values);
	
	$test_plan->update($new_values);
	
	Bugzilla->logout;
	
	# Result is zero on success, otherwise an exception will be thrown
	return 0;
}

1;