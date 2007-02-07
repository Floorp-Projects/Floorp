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
# Contributor(s): David Koenig <dkoenig@novell.com>

=head1 NAME

Bugzilla::Testopia::XmlTestCase - Testopia XmlTestCase object

=head1 DESCRIPTION

This module parsers a XML representation of a Testopia test plan,
test case, test environment, category or build and returns Testopia
objects re

=head1 SYNOPSIS

use Bugzilla::Testopia::XmlTestCase;

=cut

package Bugzilla::Testopia::XmlTestCase;
#use fields qw(testplans testcases tags categories builds);

use strict;

use Bugzilla::Product;
use Bugzilla::Testopia::Attachment;
use Bugzilla::Testopia::Build;
use Bugzilla::Testopia::Category;
use Bugzilla::Testopia::TestCase;
use Bugzilla::Testopia::TestPlan;
use Bugzilla::Testopia::TestRun;
use Bugzilla::Testopia::TestTag;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::XmlReferences;
use Bugzilla::Testopia::Product
use Bugzilla::User;
use Bugzilla::Util;

use Class::Struct;
#
# The XmlTestCase structure stores data for the verfication processing.  The database is not updated
# until a verfication pass is made through the XML data.  Some of the TestCase class references are
# database references that will not be valid until the class has been stored in the database.  This
# structure stores these references to be used during verfication and writting to the database. 
# 
struct
(
	'Bugzilla::Testopia::XmlTestCase',
	{
		attachments					=> '@',
		blocks						=> 'Bugzilla::Testopia::XmlReferences',
		category					=> '$',
		component_ids				=> '@',
		dependson					=> 'Bugzilla::Testopia::XmlReferences',
		tags						=> '@',
		testcase					=> 'Bugzilla::Testopia::TestCase',
		testplan					=> 'Bugzilla::Testopia::XmlReferences',
	}
);

sub add_attachment()
{
	my ($self,$tag) = @_;
		
	push @{$self->attachments}, $tag;
}

sub add_tag()
{
	my ($self,$tag) = @_;
		
	push @{$self->tags}, $tag;
}

=head2 get_available_products

Returns a list of products.  This is the same code as Bugzilla::Testopia::TestPlan->get_available_products
without view restrictions.

=cut

sub get_available_products {
    my $dbh = Bugzilla->dbh;
    
    my $products = $dbh->selectall_arrayref(
            "SELECT id, name 
               FROM products
           ORDER BY name", 
             {"Slice"=>{}});
    return $products;
}

sub add_component()
{
	my ($self,$component,$component_product) = @_;
	my $component_id = "";
	my $product_id = "";
	
	return "Component $component needs to provide a product." if ( $component_product eq "" );
	
	# Find the product identifier.
	my $products_ref = get_available_products();
	foreach my $product (@$products_ref)
	{
		if ( $component_product eq $product->{name} )
		{
			$product_id = $product->{id};
			last;
		}
	}
	return "Cannot find product $component_product for component $component." if ( $product_id eq "" );
	
	# Find the component identifier for the product's componet
	my $product = Bugzilla::Testopia::Product->new($product_id);
	my $components_ref = $product->components;
	foreach my $product_component ( @$components_ref )
	{
		if ( $component eq $product_component->name )
		{
			$component_id = $product_component->id;
			last;
		}
	}
	return "Product $component_product does not have a component named $component." if ( $component_id eq "" );	
	
	# Save the component identifier for this Test Case.		
	push @{$self->component_ids}, $component_id;
	
	return "";
}

sub debug_display()
{	
	my ($self) = @_;
	my $display_variable = "";

    my %text = %{$self->testcase->text()} if ( defined $self->testcase->text() );
	print STDERR "Testcase: " . $self->testcase->summary() . "\n";
	my $testcase_id = "null";
	$testcase_id = $self->testcase->id() if ( $self->testcase->id() );
	print STDERR "   ID                      " . $testcase_id . "\n";
	print STDERR "   Action\n";
	if ( defined $text{'action'} )
	{
		my @results = split(/\n/,$text{'action'});
		foreach my $result (@results)
		{
			print STDERR "      $result\n";
		}
	}
	my $alias = "null";
	$alias = $self->testcase-alias() if ( $self->testcase->alias() );
	print STDERR "   Alias                   " . $alias . "\n";
	my %author = %{$self->testcase->author()};
	my $author_id = $author{"id"};
	my $author_login = $author{"login"};
	print STDERR "   Author                  " . $author_login . " (id=" . $author_id . ", hash=" . $self->testcase->author() . ")\n";
	print STDERR "   Blocks                  " . $self->blocks->display() . "\n";
	print STDERR "   Category                " . $self->category . "\n";
	print STDERR "   Depends On              " . $self->dependson->display() . "\n";
	print STDERR "   Expected Results\n";
	if ( defined $text{'effect'} )
	{
		my @results = split(/\n/,$text{'effect'});
		foreach my $result (@results)
		{
			print STDERR "      $result\n";
		}
	}
	print STDERR "   Summary                 " . $self->testcase->summary() . "\n";
	#TODO:print STDERR "   Default Product Version " . $self->testcase->product_version() . "\n";
	#TODO:print STDERR "   Document                " . $self->testcase->text() . "\n";
	my %tester = %{$self->testcase->default_tester()};
	my $tester_id = $tester{"id"};
	my $tester_login = $tester{"login"};
	print STDERR "   Tester                  " . $tester_login . " (id=" . $tester_id . ", hash=" . $self->testcase->default_tester() . ")\n";
	print STDERR "   Is Automated            " . $self->testcase->isautomated() . "\n";
	#TODO:print STDERR "   Plans                   " . $self->testcase->plans() . "\n";
	#TODO:print STDERR "   Priority                " . $self->testcase->priority_id() . "\n";
	#TODO:print STDERR "   Product                 " . $self->testcase->product_id() . "\n";
	print STDERR "   Requirement             " . $self->testcase->requirement() . "\n";

	print STDERR "   Script                  " . $self->testcase->script() . "\n";
	print STDERR "   Script Arguments        " . $self->testcase->arguments() . "\n";
	print STDERR "   Status                  " . $self->testcase->status() . "\n";
				
	foreach my $tag (@{$self->tags})
	{
		print STDERR "   Tag                     " . $tag . "\n";
	}
		
	my @attachments = @{$self->testcase->attachments()};
	foreach my $attachment (@attachments)
	{
		my %submitter = %{$self->testcase->submitter()};
		$author_login = $author{"login"};
		print STDERR "   Attachment                     " . $attachment->description() . "\n";
		print STDERR "      Filename                    " . $attachment->filename() . "\n";
		print STDERR "      Mime Type                   " . $attachment->mime_type(). "\n";
		print STDERR "      Submitter                   " . $author_login . "\n";
	}
}

sub get_testcase_ids()
{
	my ($self, $field, @new_testcases) = @_;
	my $error_message = "";
	
	my @testcase_id = @{$self->$field->get(uc $Bugzilla::Testopia::Xml::DATABASE_ID)};
	
	foreach my $testcase_summary ( @{$self->$field->get(uc $Bugzilla::Testopia::Xml::XML_DESCRIPTION)} )
	{
		foreach my $testcase (@new_testcases)
		{
			push @testcase_id, $testcase->testcase->id() if ( $testcase->testcase->summary() eq $testcase_summary );
		}
	}
	
	#TODO Testplans using Database_Description
	foreach my $testcase_summary ( @{$self->$field->get(uc $Bugzilla::Testopia::Xml::DATABASE_DESCRIPTION)} )
	{
			$error_message .= "\n" if  ( $error_message ne "" );
			$error_message .= "Have not implemented code for $Bugzilla::Testopia::Xml::DATABASE_DESCRIPTION lookup for blocking test case " . $testcase_summary . "' for Test Case '". $self->testcase->summary . "'.";
	}
	return $error_message if ( $error_message ne "" );
	
	my @return_testcase_id;
	foreach my $testcase_id (@testcase_id)
	{
		my $testcase = Bugzilla::Testopia::TestCase->new($testcase_id);
		if ( ! defined($testcase) )
		{
			$error_message .= "\n" if  ( $error_message ne "" );
			$error_message .= "Could not find blocking Test Case '" . $testcase_id . "' for Test Case '". $self->testcase->summary . "'.";
		}
		else
		{
			push @return_testcase_id, $testcase->id();
		}
	}
	return $error_message if ( $error_message ne "" );
	
	return @return_testcase_id;
}

=head2 store

Saves a imported Test Case.  This method insures that all Test Case attributes not stored
in the Test Case object are created. The attributes include the Test Plan, tags, compoents,
attachments and categories.

=cut

sub store()
{
	my ($self, @new_testplans) = @_;
	my $error_message = "";
	my @testplan_id = @{$self->testplan->get(uc $Bugzilla::Testopia::Xml::DATABASE_ID)};
	
	# If we have any references to test plans from the XML data we need to search the @new_testplans
	# array to find the actual test plan id.  The new_testplans array contains all test plans created
	# from the XML.
	# Order of looping does not matter, number of test plans associated to each test case should be small.
	foreach my $testplan_name ( @{$self->testplan->get(uc $Bugzilla::Testopia::Xml::XML_DESCRIPTION)} )
	{
		foreach my $testplan (@new_testplans)
		{
			push @testplan_id, $testplan->id() if ( $testplan->name() eq $testplan_name );
		}
	}
	
	#TODO Testplans using Database_Description
	foreach my $testplan_name ( @{$self->testplan->get(uc $Bugzilla::Testopia::Xml::DATABASE_DESCRIPTION)} )
	{
			$error_message .= "\n" if  ( $error_message ne "" );
			$error_message .= "Have not implemented code for $Bugzilla::Testopia::Xml::DATABASE_DESCRIPTION lookup of test plan " . $testplan_name . "' for Test Case '". $self->testcase->summary . "'.";
	}
	return $error_message if ( $error_message ne "" );
	
	# Have to have a testplan to determine valid categories for testcase.
	return "Test Case '" . $self->testcase->summary . "' needs a Test Plan." if ( $#testplan_id == -1 );
	
	# Verify that each testplan exists.
	my @testplan;
	foreach my $testplan_id (@testplan_id)
	{
		my $testplan = Bugzilla::Testopia::TestPlan->new($testplan_id);
		if ( ! defined($testplan) )
		{
			$error_message .= "\n" if  ( $error_message ne "" );
			$error_message .= "Could not find Test Plan '" . $testplan_id . "' for Test Case '". $self->testcase->summary . "'.";
		}
		else
		{
			push @testplan, $testplan;
		}
	}
	return $error_message if ( $error_message ne "" );
	
	# Verify that each testplan has the testcase's category associated to it.  If the category does not
	# exist it will be created.
	foreach my $testplan (@testplan)
	{
		my $category = $testplan->product->categories->[0];
		my $categoryid = $category->check_name($self->category) if ( defined($category) );
    	if ( ! defined($categoryid) )
    	{
    		my $new_category = Bugzilla::Testopia::Category->new({
    			product_id  => $testplan->product_id,
				name        => $self->category,
				description => "FIX ME.  Created during Test Plan import."
			});
			$categoryid = $new_category->store();
    	}
    	$self->testcase->{'category_id'} = $categoryid if ( ! defined($self->testcase->{'category_id'}) );
    }
	my $case_id = $self->testcase->store();
	$self->testcase->{'case_id'} = $case_id;
	foreach my $attachment ( @{$self->attachments} )
	{
		$attachment->{'case_id'} = $case_id;
		$attachment->store();
	}
	foreach my $asciitag ( @{$self->tags} )
	{
		my $classtag = Bugzilla::Testopia::TestTag->new({'tag_name' => $asciitag});
		my $tagid = $classtag->store;
		$self->testcase->add_tag($tagid);
	}
	
	# Link the testcase to each of it's testplans.
	foreach my $testplan ( @testplan )
	{
		$self->testcase->link_plan($testplan->id(),$case_id)
	}
	
	# Code below requires the testplans to be linked into testcases before being run.
	foreach my $component_id ( @{$self->component_ids} )
	{
		$self->testcase->add_component($component_id);
	}
	
	return $error_message;
}

=head2 store_relationships

Save the dependson and blocks relationships between Test Cases.  This method can only be
called after the Test Cases being imported have been stored.  The dependson and blocks
relationships use the Test Case identifier which is created only after the Test Case has
been stored.

=cut

sub store_relationships()
{
	my ($self, @new_testcases) = @_;

	# Hashes are used because the entires in blocks and dependson must be unique.
	my %blocks = ();
	foreach my $block ( $self->get_testcase_ids("blocks",@new_testcases) )
	{
		$blocks{$block}++;
	}
	my $blocks_size = keys( %blocks );
	my %dependson = ();
	foreach my $dependson ( $self->get_testcase_ids("dependson",@new_testcases) )
	{
		$dependson{$dependson}++;
	}
	my $dependson_size = keys( %dependson );
			

	if ( ( $blocks_size > 0 ) || ( $dependson_size > 0 ) )
	{
		# Need to add the current blocks and dependson from the Test Case; otherwise, they will
		# be removed.
    	foreach my $block ( split(/ /,$self->testcase->blocked_list_uncached()) )
		{
			$blocks{$block}++;
		}
		foreach my $dependson ( split(/ /,$self->testcase->dependson_list_uncached()) )
		{
			$dependson{$dependson}++;
		}
		my @blocks = keys(%blocks);
		my @dependson = keys(%dependson);
		$self->testcase->update_deps( join(' ',@dependson ),join(' ',@blocks) );
	}
}

1;
