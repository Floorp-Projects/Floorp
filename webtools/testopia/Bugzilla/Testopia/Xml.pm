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

Bugzilla::Testopia::Xml - Testopia Xml object

=head1 DESCRIPTION

This module parsers a XML representation of a Testopia test plan,
test case, test environment, category or build and returns Testopia
objects re

=head1 SYNOPSIS

use Bugzilla::Testopia::Xml;

=cut

package Bugzilla::Testopia::Xml;
#use fields qw(testplans testcases tags categories builds);

use strict;

use Bugzilla::Config;
use Bugzilla::Error;
use Bugzilla::Product;
use Bugzilla::Testopia::Attachment;
use Bugzilla::Testopia::Build;
use Bugzilla::Testopia::Category;
use Bugzilla::Testopia::TestCase;
use Bugzilla::Testopia::TestPlan;
use Bugzilla::Testopia::TestRun;
use Bugzilla::Testopia::TestTag;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::XmlTestCase;
use Bugzilla::User;
use Bugzilla::Util;

use Class::Struct;
#
# The Xml structure is used to keep track of all new Testopia objects being created.
#
struct
(
	'Bugzilla::Testopia::Xml',
	{
		attachments 		=> '@',
		builds 				=> '@',
		categories 			=> '@',
		tags		 		=> '@',
		testenvironments 	=> '@',
		testcases 			=> '@',
		testplans 			=> '@',
		parse_error 		=> '$',
	}
);

#TODO: Add this to checksetup
use Text::Diff;

#use base qw(Exporter);


###############################
####    Initialization     ####
###############################

=head1 FIELDS

=cut

###############################
####       Methods         ####
###############################

=head2 new

Instantiate a new xml object.  This takes a single argument

Instantiate a new test plan. This takes a single argument 
either a test plan ID or a reference to a hash containing keys 
identical to a test plan's fields and desired values.

=cut

#sub new {
#   my Bugzilla::Testopia::Xml $self = fields::new(ref($invocant) || $invocant);
# 	$self->{testplans} = [];
# 	$self->{testcases} = [];
# 	$self->{tags} = [];
# 	$self->{categories} = [];
# 	$self->{builds} = [];
#    return $self;
#}

sub debug_display()
{	
	my ($self) = @_;
	my $attachments_array_ref = $self->attachments;
	my $categories_array_ref = $self->categories;
	my $testcases_array_ref = $self->testcases;
	my $testplans_array_ref = $self->testplans;

	foreach my $category ( @$categories_array_ref )
	{
		bless($category,"Bugzilla::Testopia::Category");
		
		print STDERR "Category: " . $category->name() . "\n";
		my $category_id = "null";
		$category_id = $category->id() if ( $category->id() );
		print STDERR "   ID                      " . $category_id . "\n";
		print STDERR "   Product ID              " . $category->product_id() . "\n";
		print STDERR "   Description             " . $category->description() . "\n";
	}

	foreach my $testplan ( @$testplans_array_ref )
	{
		bless($testplan,"Bugzilla::Testopia::TestPlan");
		
		print STDERR "Testplan: " . $testplan->name() . "\n";
		my $testplan_id = "null";
		$testplan_id = $testplan->id() if ( $testplan->id() );
		print STDERR "   ID                      " . $testplan_id . "\n";
		my %author = %{$testplan->author()};
		my $author_id = $author{"id"};
		my $author_login = $author{"login"};
		print STDERR "   Author                  " . $author_login . " (id=" . $author_id . ", hash=" . $testplan->author() . ")\n";
		print STDERR "   Creation Date           " . $testplan->creation_date() . "\n";
		print STDERR "   Description             " . $testplan->text() . "\n";
		print STDERR "   Default Product Version " . $testplan->product_version() . "\n";
		print STDERR "   Document                " . $testplan->text() . "\n";
		my %editor = %{$testplan->editor()};
		my $editor_id = $editor{"id"};
		my $editor_login = $editor{"login"};
		print STDERR "   Editor                  " . $editor_login . " (id=" . $editor_id . ", hash=" . $testplan->editor() . ")\n";
		print STDERR "   Is Active               " . $testplan->isactive() . "\n";
		print STDERR "   Product                 " . $testplan->product_id() . "\n";
		print STDERR "   Type                    " . $testplan->type_id() . "\n";
		
		foreach my $tag ( @{$self->tags} )
		{
			print STDERR "   Tag                     " . $tag . "\n";
		}
		
		my @attachments = @{$testplan->attachments()};
		foreach my $attachment (@attachments)
		{
			my %submitter = %{$testplan->submitter()};
			my $author_login = $author{"login"};
			print STDERR "   Attachment                     " . $attachment->description() . "\n";
			print STDERR "      Creation Date               " . $attachment->creation_ts(). "\n";
			print STDERR "      Filename                    " . $attachment->filename() . "\n";
			print STDERR "      Mime Type                   " . $attachment->mime_type(). "\n";
			print STDERR "      Submitter                   " . $author_login . "\n";
		}
	}
	
	foreach my $testcase ( @$testcases_array_ref )
	{
		bless($testcase,"Bugzilla::Testopia::XmlTestCase");
		$testcase->debug_display();
	}
}

sub error()
{
	my ($self, $message) = @_;
	
	print STDERR $message . "\n";
	
	$self->parse_error("TRUE");
}

sub parse()
{
	my ($self, $xml) = @_;
	my $twig = XML::Twig->new();								  
	$twig->parse($xml);
	my $root = $twig->root;
	
	my $attachments_array_ref = $self->attachments;
	my $categories_array_ref = $self->categories;
	my $testplans_array_ref = $self->testplans;
	my $testcases_array_ref = $self->testcases;
	
	foreach my $twig_category ($root->children('category'))
	{
		my $product = new Bugzilla::Product({name => $twig_category->field('product')});
		if ( ! $product )
		{
			$self->error("Cannot find product '" . $twig_category->field('product') . "' for category '" . $twig_category->field('name') . "'.");
			$self->{"parser_error"} = 1;
			next;
		}
		my $category = new Bugzilla::Testopia::Category
		({
			name => $twig_category->field('name'),
			product_id => $product->id(),
			description => $twig_category->field('description'),
		});
		push @$categories_array_ref, $category;
	}
	
	my $testplan = Bugzilla::Testopia::TestPlan->new({ 'name' => 'dummy' });
	my %plantype_ids;
	my @temparray = @{$testplan->get_plan_types()};
	foreach my $arrayelement (@temparray)
	{
		my %temphash = %{$arrayelement};	
		$plantype_ids{$temphash{"name"}} = $temphash{"id"};
	}
	
	foreach my $twig_testplan ($root->children('testplan'))
	{
		my $author = $twig_testplan->field('author');
		# Bugzilla::User::match returns a array with a user hash.  Fields of the hash needed
		# are 'id' and 'login'.
		my $author_ref = Bugzilla::User::match($author, 1, 0);
		my $author_id = -1;
		if ( ! $author_ref->[0] )
		{
			$self->error("Cannot find author '" . $author . "' in test plan '" . $twig_testplan->field('name') . "'.");
		}
		else
		{
			my $author_user = $author_ref->[0];
			bless($author_user,"Bugzilla::User");
			$author_id = $author_user->id();
		}
		my $editor = $twig_testplan->field('editor');
		my $editor_ref = Bugzilla::User::match($editor, 1, 0);
		my $editor_id = -1;
		if ( ! $editor_ref->[0] )
		{
			$self->error("Cannot find editor '" . $editor . "' in test plan '" . $twig_testplan->field('name') . "'.");
		}
		else
		{
			my $editor_user = $editor_ref->[0];
			bless($editor_user,"Bugzilla::User");
			$editor_id = $editor_user->id();
		}
		$testplan = Bugzilla::Testopia::TestPlan->new({
            'name'       => $twig_testplan->field('name'),
            'product_id' => Bugzilla::Testopia::TestPlan::lookup_product_by_name($twig_testplan->field('product')),
            'default_product_version' => $twig_testplan->field('productversion'),
            'type_id'    => $plantype_ids{$twig_testplan->field('type')},
            'text'       => $twig_testplan->field('document'),
            'author_id'  => $author_id,
            'editor_id'  => $editor_id,
            'isactive'  => $twig_testplan->field('archive'),
            'creation_date' => $twig_testplan->field('created')
    	});
		push @$testplans_array_ref, $testplan;
		
		my @tags = $twig_testplan->children('tag');
		foreach my $twig_tag (@tags)
		{
			push @{$self->tags}, $twig_tag->text();
		}
		
		my @attachments = $twig_testplan->children('attachment');
		foreach my $twig_attachments (@attachments)
		{
			my $attachment = Bugzilla::Testopia::Attachment->new({
            	'description'  => $twig_attachments->field('description'),
            	'filename'     => $twig_attachments->field('filename'),
            	'submitter_id' => $twig_attachments->field('submitter'),
            	'mime_type'    => $twig_attachments->field('mimetype'),
            	'creation_ts'  => $twig_attachments->field('created'),
            	'data'         => $twig_attachments->field('data')
    		});
			push @$attachments_array_ref, $attachment;
			#$testplan->add_tag($tag->id());
		}
	}
	
	my $testcase = Bugzilla::Testopia::TestCase->new({ 'name' => 'dummy' });
	my %priority_ids;
	@temparray = @{$testcase->get_priority_list()};
	foreach my $arrayelement (@temparray)
	{
		my %temphash = %{$arrayelement};
		my $longname = $temphash{"name"};
		# The long name.  "P1 - Urgent"
		$priority_ids{$longname} = $temphash{"id"};
		# The short name.  "P1"
		my $shortname = $longname;
		$shortname =~ s/ - .*//;
		$priority_ids{$shortname} = $temphash{"id"} if ( $longname ne $shortname );
	}
	foreach my $twig_testcase ($root->children('testcase'))
	{
		my $author = $twig_testcase->field('author');
		# Bugzilla::User::match returns a array with a user hash.  Fields of the hash needed
		# are 'id' and 'login'.
		my $author_ref = Bugzilla::User::match($author, 1, 0);
		my $author_id = -1;
		if ( ! $author_ref->[0] )
		{
			$self->error("Cannot find author '" . $author . "' in test case '" . $twig_testcase->field('summary') . "'.");
		}
		else
		{
			my $author_user = $author_ref->[0];
			bless($author_user,"Bugzilla::User");
			$author_id = $author_user->id();
		}
		my $tester = $twig_testcase->field('defaulttester');
		# Bugzilla::User::match returns a array with a user hash.  Fields of the hash needed
		# are 'id' and 'login'.
		my $tester_ref = Bugzilla::User::match($tester, 1, 0);
		my $tester_id = -1;
		if ( ! $tester_ref->[0] )
		{
			$self->error("Cannot find default tester '" . $tester . "' in test case '" . $twig_testcase->field('summary') . "'.");
		}
		else
		{
			my $tester_user = $tester_ref->[0];
			bless($tester_user,"Bugzilla::User");
			$tester_id = $tester_user->id();
		}
		my $status_id = Bugzilla::Testopia::TestCase::lookup_status_by_name($twig_testcase->field('status'));
		$self->error("Cannot find status '" . $twig_testcase->field('status') . "' in test case '" . $twig_testcase->field('summary') . "'.") if ( ! defined($status_id) );
			
		my $xml_testcase = new Bugzilla::Testopia::XmlTestCase;
		$xml_testcase->testcase(Bugzilla::Testopia::TestCase->new({
    		'action'     		=> $twig_testcase->field('action'),
            'alias'          	=> $twig_testcase->field('alias') || undef,
            'arguments'      	=> $twig_testcase->field('arguments'),
            'author_id'  		=> $author_id,
            #TODO: Blocks
            'blocks'     		=> undef,
            
            'case_status_id' 	=> $status_id,
            'category_id'    	=> undef,
            'creation_date'		=> $twig_testcase->field('created'),
            'default_tester_id' => $tester_id,
            #TODO: Depends On
            'dependson'  		=> undef,
            'effect'     		=> $twig_testcase->field('effect'),
            'expectedresults'	=> $twig_testcase->field('expectedresults'),
            'isautomated'    	=> $twig_testcase->field('isautomated'),
            'plans'      		=> undef,
            'priority_id'    	=> $priority_ids{$twig_testcase->field('priority')},
            'requirement'    	=> $twig_testcase->field('requirement'),
            'script'         	=> $twig_testcase->field('script'),
            'summary'        	=> $twig_testcase->field('summary'),
    	}));
    	# Action field is html format.
    	my $action = $twig_testcase->field('action');
    	$action =~ s/\n/<br>/g;
    	$xml_testcase->action($action);
    	$xml_testcase->category($twig_testcase->field('category'));
    	# Expectedresults field is html format.
    	my $expectedresults = $twig_testcase->field('expectedresults');
    	$expectedresults =~ s/\n/<br>/g;
    	$xml_testcase->expectedresults($expectedresults);
    	push @$testcases_array_ref, $xml_testcase;
    	
    	my @tags = $twig_testcase->children('tag');
		foreach my $twig_tag (@tags)
		{
				$xml_testcase->add_tag($twig_tag->text());
		}
    
    	#my @components;
    	#foreach my $id (@comps){
        #	detaint_natural($id);
        #	validate_selection($id, 'id', 'components');
        #	push @components, $id;
    	#}
    
    	#$case->add_component($_) foreach (@components);
	}
	
	#TODO Verfication of data.
	#Test cases require a category or store will fail.
	
	# Don't really care about the value of parse_error.  If it has been defined then a error occurred
	# parsing the XML.
	if ( ! defined $self->parse_error )
	{
#		$self->debug_display();
		my @testplanarray;
		
		foreach my $testplan ( @{$self->testplans} )
		{
			my $plan_id = $testplan->store();
			$testplan->{'plan_id'} = $plan_id;
			foreach my $asciitag ( @{$self->tags} )
			{
				my $classtag = Bugzilla::Testopia::TestTag->new({'tag_name' => $asciitag});
				my $tagid = $classtag->store;
				$testplan->{'tag_id'} = $tagid;
				$testplan->add_tag($tagid);
			}
			
			push @testplanarray, $testplan;
		}
		
		foreach my $testcase ( @{$self->testcases} )
		{
			bless($testcase,"Bugzilla::Testopia::XmlTestCase");
			my $result = $testcase->store($testplan->{'author_id'},@testplanarray);
			if ( $result ne "" )
			{
				$self->error($result);
			}
		}
	}
	$twig->purge;
}

=head1 TODO

Use Bugzilla::Product and Version in 2.22

=head1 SEE ALSO

Testopia::(TestRun, TestCase, Category, Build, Evnironment)

=head1 AUTHOR

David Koenig <dkoenig@novell.com>

=cut

1;
