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
#use base qw(Exporter);

use Bugzilla::Config;
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

use constant AUTOMATIC => "AUTOMATIC";
use constant BLOCKS => "blocks";
our $DATABASE_DESCRIPTION = "Database_descripiton";
our $DATABASE_ID = "Database_id";
use constant IGNORECASE => 1;
use constant DEPENDSON => "dependson";
use constant PCDATA => "#PCDATA";
our $TESTOPIA_GT = "&testopia_gt;";
our $TESTOPIA_LT = "&testopia_lt;";
use constant TESTPLAN_REFERENCE => "testplan_reference";
our $XML_DESCRIPTION = "Xml_description";
our $XML_AMP = "&[Aa][Mm][Pp];";
our $XML_APOS = "&[Aa][Pp][Oo][Ss];";
our $XML_GT = "&[Gg][Tt];";
our $XML_LT = "&[Ll][Tt];";
our $XML_QUOT = "&[Qq][Uu][Oo][Tt];";

use constant XMLREFERENCES_FIELDS => "Database_descripiton Database_id Xml_description";
@Bugzilla::Testopia::Xml::EXPORT = qw($DATABASE_DESCRIPTION $DATABASE_ID $XML_DESCRIPTION);


use Class::Struct;
#
# The Xml structure is used to keep track of all new Testopia objects being created.
#
struct
(
	'Bugzilla::Testopia::Xml',
	{
		# Array of attachments read for xml source.
		attachments 		=> '@',
#TODO	builds 				=> '@',
		# Array of categories read from xml source.  Categories 
		categories 			=> '@',
		tags		 		=> '@',
#TODO	testenvironments 	=> '@',
		# Array of testcases read from xml source.
		testcases 			=> '@',
		# Hash of testcase aliases indexed by testcase id.  Used during verfication to
		# insure that alias does not exist and that new aliases are used in only one testcase.
		testcase_aliases	=> '%',
		# Array of testplans read from xml source.
		testplans 			=> '@',
		# If true indicates some type of error has occurred processing the XML.  Used to prevent
		# updating Testopia with contents of current XML.
		parse_error 		=> '$',
	}
);

#TODO: Add this to checksetup
use Text::Diff;



###############################
####    Initialization     ####
###############################

=head1 FIELDS

=cut

###############################
####       Methods         ####
###############################

sub debug_display()
{	
	my ($self) = @_;

	foreach my $category ( @{$self->categories})
	{
		bless($category,"Bugzilla::Testopia::Category");
		
		print STDERR "Category: " . $category->name() . "\n";
		my $category_id = "null";
		$category_id = $category->id() if ( $category->id() );
		print STDERR "   ID                      " . $category_id . "\n";
		print STDERR "   Product ID              " . $category->product_id() . "\n";
		print STDERR "   Description             " . $category->description() . "\n";
	}

	foreach my $testplan ( @{$self->testplans} )
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
	
	foreach my $testcase ( @{$self->testplans} )
	{
		bless($testcase,"Bugzilla::Testopia::XmlTestCase");
		$testcase->debug_display();
	}
}

=head2 entity_replace_testopia

Replace testopia entities in the string supplied.  

This method is called for any field that is stored in HTML format.  The Testopia entities are 
provided to allow users to pass HTML tags.

For example, if you want to store a < in Bold font, the following XML

&testopia_lt;B&testopia_gt;&lt;&testopia_lt;/B&testopia_gt;

is passed to the HTML field as

<B>&lt;</B>

=cut

sub entity_replace_testopia
{
	my ($string) = @_;
	
	return undef if ( ! defined $string );
	
	$string =~ s/$TESTOPIA_GT/>/g;
	$string =~ s/$TESTOPIA_LT/</g;
	$string =~ s/\n/<br>/g;
	
	return $string;
}

=head2 entity_replace_xml

Replace xml entities in the string supplied.

This method is called for any field that is not stored in HTML format.  The source is XML so any
XML entity will be in the &<name>; format.  These entities need to be converted back to their
character representation.

=cut

sub entity_replace_xml
{
	my ($string) = @_;
	
	return undef if ( ! defined $string );
	
	$string =~ s/$XML_GT/>/g;
	$string =~ s/$XML_LT/</g;
	$string =~ s/$XML_AMP/&/g;
	$string =~ s/$XML_APOS/'/g;
	$string =~ s/$XML_QUOT/"/g;
	
	return $string;
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

	my $twig = XML::Twig->new( load_DTD => 1, keep_encoding => 1 );	

	$twig->parse($xml);
	my $root = $twig->root;
	
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
		push @{$self->categories}, $category;
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
		my $author = $twig_testplan->att('author');
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

		my $product_id = Bugzilla::Testopia::TestPlan::lookup_product_by_name($twig_testplan->field('product'));
		if ( ! defined($product_id) )
		{
			$self->error("Cannot find product '" . $twig_testplan->field('product') . "' in test plan '" . $twig_testplan->field('name') . "'.");
		}
		
		$testplan = Bugzilla::Testopia::TestPlan->new({
            'name'						=> entity_replace_xml($twig_testplan->field('name')),
            'product_id'				=> $product_id,
            'default_product_version'	=> entity_replace_xml($twig_testplan->field('productversion')),
            'type_id'					=> $plantype_ids{$twig_testplan->att('type')},
            'text'						=> entity_replace_testopia($twig_testplan->field('document')),
            'author_id'					=> $author_id,
			'isactive'					=> entity_replace_xml($twig_testplan->field('archive')),
			'creation_date'				=> entity_replace_xml($twig_testplan->field('created'))
    	});
		push @{$self->testplans}, $testplan;
		
		my @tags = $twig_testplan->children('tag');
		foreach my $twig_tag (@tags)
		{
			push @{$self->tags}, entity_replace_xml($twig_tag->text());
		}
		
		my @attachments = $twig_testplan->children('attachment');
		foreach my $twig_attachments (@attachments)
		{
			my $attachment = Bugzilla::Testopia::Attachment->new({
            	'description'  => entity_replace_xml($twig_attachments->field('description')),
            	'filename'     => entity_replace_xml($twig_attachments->field('filename')),
            	'submitter_id' => entity_replace_xml($twig_attachments->field('submitter')),
            	'mime_type'    => entity_replace_xml($twig_attachments->field('mimetype')),
            	'creation_ts'  => entity_replace_xml($twig_attachments->field('created')),
            	'data'         => entity_replace_xml($twig_attachments->field('data'))
    		});
			push @{$self->attachments}, $attachment;
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
		my $summary = entity_replace_xml($twig_testcase->field('summary')) || undef;
		my $author = $twig_testcase->att('author');
		# Bugzilla::User::match returns a array with a user hash.  Fields of the hash needed
		# are 'id' and 'login'.
		my $author_ref = Bugzilla::User::match($author, 1, 0);
		my $author_id = -1;
		if ( ! $author_ref->[0] )
		{
			$self->error("Cannot find author '" . $author . "' in test case '" . $summary . "'.");
		}
		else
		{
			my $author_user = $author_ref->[0];
			bless($author_user,"Bugzilla::User");
			$author_id = $author_user->id();
		}
		my $tester = entity_replace_xml($twig_testcase->field('defaulttester'));
		# Bugzilla::User::match returns a array with a user hash.  Fields of the hash needed
		# are 'id' and 'login'.
		my $tester_ref = Bugzilla::User::match($tester, 1, 0);
		my $tester_id = -1;
		if ( ! $tester_ref->[0] )
		{
			$self->error("Cannot find default tester '" . $tester . "' in test case '" . $summary . "'.");
		}
		else
		{
			my $tester_user = $tester_ref->[0];
			bless($tester_user,"Bugzilla::User");
			$tester_id = $tester_user->id();
		}
		my $status_id = Bugzilla::Testopia::TestCase::lookup_status_by_name($twig_testcase->att('status'));
		$self->error("Cannot find status '" . $twig_testcase->att('status') . "' in test case '" . $summary . "'.") if ( ! defined($status_id) );
			
		my $xml_testcase = new Bugzilla::Testopia::XmlTestCase;
		$xml_testcase->blocks(Bugzilla::Testopia::XmlReferences->new(IGNORECASE, XMLREFERENCES_FIELDS));
		$xml_testcase->dependson(Bugzilla::Testopia::XmlReferences->new(IGNORECASE, XMLREFERENCES_FIELDS));
		$xml_testcase->testplan(Bugzilla::Testopia::XmlReferences->new(IGNORECASE, XMLREFERENCES_FIELDS));
		push @{$self->testcases}, $xml_testcase;
		my $alias = entity_replace_xml($twig_testcase->field('alias')) || undef;

		$xml_testcase->testcase(Bugzilla::Testopia::TestCase->new({
			'action'			=> entity_replace_testopia($twig_testcase->field('action')),
            'alias'          	=> $alias,
            'arguments'      	=> entity_replace_xml($twig_testcase->field('arguments')),
            'author_id'  		=> $author_id,
            'blocks'     		=> undef,
            'breakdown'         => entity_replace_testopia($twig_testcase->field('breakdown')),
            'case_status_id' 	=> $status_id,
            'category_id'    	=> undef,
            'creation_date'		=> $twig_testcase->att('created'),
            'default_tester_id' => $tester_id,
            'dependson'  		=> undef,
            'effect'			=> entity_replace_testopia($twig_testcase->field('expectedresults')),
            'isautomated'    	=> ( uc $twig_testcase->att('automated') ) eq AUTOMATIC,
            'plans'      		=> undef,
            'priority_id'    	=> $priority_ids{$twig_testcase->att('priority')},
            'requirement'    	=> entity_replace_xml($twig_testcase->field('requirement')),
            'setup'         	=> entity_replace_testopia($twig_testcase->field('setup')),
            'script'         	=> entity_replace_xml($twig_testcase->field('script')),
            'summary'        	=> $summary,
    	}));
    	foreach my $twig_testplan_reference ( $twig_testcase->children(TESTPLAN_REFERENCE) )
    	{
			my $testplan_reference = $twig_testplan_reference->children_text(PCDATA);
			if ( $testplan_reference eq "" )
			{
				$self->error("No test plan included for type '" 
    			             . $twig_testplan_reference->att('type') 
    			             . "' in test case '" . $twig_testcase->field('summary') . "'." );
    		}
	   		elsif ( ! $xml_testcase->testplan->add($twig_testplan_reference->att('type'),entity_replace_xml($testplan_reference)) )
    		{
    			$self->error("Do not know how to handle test plan of type '" 
    			             . $twig_testplan_reference->att('type') 
    			             . "' in test case '" . $twig_testcase->field('summary') . "'." 
    			             . "\nKnow types are: (" . uc XMLREFERENCES_FIELDS . ").");
    		}
    	}
    	# Keep track of this testcase's alias.  Used during verification to insure aliases are unique.
    	$self->testcase_aliases(entity_replace_xml($twig_testcase->field('summary')),$alias) if ( defined $alias );
    	# Keep track of this testcase's category.  To create a category at this time would require
    	# getting the product from the Test Plan that this Test Case is associated with.  The category
    	# will created when each Test Case is stored.
     	$xml_testcase->category(entity_replace_xml($twig_testcase->field('categoryname')));
 
    	my @tags = $twig_testcase->children('tag');
		foreach my $twig_tag ( @tags )
		{
			$xml_testcase->add_tag(entity_replace_xml($twig_tag->text()));
		}
    
    	my @components = $twig_testcase->children('component');
    	foreach my $twig_component ( @components )
		{
			$xml_testcase->add_component(entity_replace_xml($twig_component->text()));
    	}
    	
    	foreach my $twig_blocks ( $twig_testcase->children(BLOCKS) )
    	{
	   		if ( ! $xml_testcase->blocks->add($twig_blocks->att('type'),entity_replace_xml($twig_blocks->children_text(PCDATA))) )
    		{
    			$self->error("Do not know how to handle a blocking test case of type '" . $twig_blocks->att('type') . "' in test case '" . $xml_testcase->testcase->summary() . "'.") 
    		}
    	}

    	foreach my $twig_dependson ( $twig_testcase->children(DEPENDSON) )
    	{
	   		if ( ! $xml_testcase->dependson->add($twig_dependson->att('type'),entity_replace_xml($twig_dependson->children_text(PCDATA))) )
    		{
    			$self->error("Do not know how to handle dependency of type '" . $twig_dependson->att('type') . "' in test case '" . entity_replace_xml($xml_testcase->testcase->summary()) . "'.") 
    		}
    	}
   	}
	
	#
	# Start of data integrity check.
	#
	# Run through the Test Plans and Test Cases looking for integrity errors.
	#
		
	# Check for duplicate aliases.  Loop though all testcases that have a alias.
	my %used_alias;
	my %duplicate_alias;
	foreach my $summary ( keys %{$self->testcase_aliases} )
	{
		# Get the alias.
		my $alias = $self->testcase_aliases($summary);
		# Is the alias used by a testcase in the database already?  If so add it to the duplicate list
		# and move onto next testcase.
		my $alias_testcase_id = Bugzilla::Testopia::TestCase::class_check_alias($alias);
		if ( $alias_testcase_id )
		{
			$duplicate_alias{$alias} = $alias_testcase_id;
			next;
		}
		# Is the alias in the used_alias array?  
		if ( defined( $used_alias{$alias} ) )
		{
			# If so then another testcase being created also used the alias.  Add the alias to the
			# duplicate list.
			$duplicate_alias{$alias} = "import";
		}
		else
		{
			# Alias has not been seen yet.  Add it to the used_alias list to keep track of it.
			$used_alias{$alias} = "";
		}
	}
	# The @duplicate_alias list contains aliases used by more that one test case.  Display them and set
	# error condition
	foreach my $summary ( keys %{$self->testcase_aliases} )
	{
		my $alias = $self->testcase_aliases($summary);
		if ( exists $duplicate_alias{$alias} )
		{
			my $error_message = "Test Case '" . $summary . "' has a non-unique alias '" . $alias . "'.";
			if ( $duplicate_alias{$alias} ne "import" )
			{
				$error_message .= "  Test Case " . $duplicate_alias{$alias} . " already uses the alias '" . $alias . "'.";
			}
			else
			{
				$error_message .= "  Additional test cases being imported are using the alias '" . $alias . "'.";
			}
			$self->error($error_message);
		}
	}
	
	#
	# Start of data store.
	#
	# No data has been written prior to this point.  If parse_error has not been set the XML is valid
	# and no integrity errors were found.  It's time to start storing the Test Plans and Test Cases.
	#
		
	if ( ! defined $self->parse_error )
	{
		foreach my $testplan ( @{$self->testplans} )
		{
			my $plan_id = $testplan->store();
			$testplan->{'plan_id'} = $plan_id;
			print "Created Test Plan $plan_id: " . $testplan->name() . "\n";
			foreach my $asciitag ( @{$self->tags} )
			{
				my $classtag = Bugzilla::Testopia::TestTag->new({'tag_name' => $asciitag});
				my $tagid = $classtag->store;
				$testplan->{'tag_id'} = $tagid;
				$testplan->add_tag($tagid);
			}
		}
		
		# Store the testcases.
		foreach my $testcase ( @{$self->testcases} )
		{
			bless($testcase,"Bugzilla::Testopia::XmlTestCase");
			my $result = $testcase->store(@{$self->testplans});
			if ( $result ne "" )
			{
				$self->error($result);
			}
			else
			{
				print "Created Test Case " . $testcase->testcase->id() . ": " . $testcase->testcase->summary() . "\n";
			}
		}
		# Now that each testcase has been stored we loop though them again and create
		# relationships like blocks or dependson.
		foreach my $testcase ( @{$self->testcases} )
		{
			$testcase->store_relationships(@{$self->testcases});
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
