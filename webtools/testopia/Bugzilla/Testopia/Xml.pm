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

This module parsers a XML representation of a Testopia Test Plans,
Test Cases, or Categories and stores them in Testopia if not errors
are detected.

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

###############################
####    Initialization     ####
###############################

=head1 CONSTANTS

=over 4

=item STRIP_NONE  - Do not strip white space from string.

=item STRIP_LEFT  - Strip white space from left side of string.

=item STRIP_RIGHT - Strip white space from right side of string.

=item STRIP_BOTH  - Strinp white space from left and right side of string.

=back

=cut

use constant AUTOMATIC => "AUTOMATIC";
use constant BLOCKS => "blocks";
our $DATABASE_DESCRIPTION = "Database_descripiton";
our $DATABASE_ID = "Database_id";
use constant IGNORECASE => 1;
use constant DEPENDSON => "dependson";
use constant PCDATA => "#PCDATA";
use constant REQUIRE => "REQUIRE";
use constant STRIP_NONE => 0;
use constant STRIP_LEFT => 1;
use constant STRIP_RIGHT => 2;
use constant STRIP_BOTH => 3;
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
		# Hash of testcase aliases indexed by testcase summary.  Used during verfication to
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
####       Methods         ####
###############################

=head1 METHODS

=over 4

=item C<entity_replace_testopia($string,$strip)>

 Description:  This method is called for any field that is stored in HTML format.  The Testopia
               entities are provided to allow users to pass HTML tags.  For example, if you want to
               store a < in Bold font, the following XML
                  &testopia_lt;B&testopia_gt;&lt;&testopia_lt;/B&testopia_gt;
               is passed to the HTML field as
                  <B>&lt;</B>
 Params:       $string - String to convert.
 Returns:      converted string.

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

=pod

=item C<entity_replace_xml($string,$strip)>

 Description:  This method is called for any field that is not stored in HTML format.  The source
               is XML so any XML entity will be in the &<name>; format.  These entities need to be
               converted back to their character representation.
 Params:       $string - String to convert.
               $strip - STRIP_NONE, STRIP_LEFT, STRIP_RIGHT, or STRIP_BOTH.  White space stripping
                        is included because MySQL 5.0.3 retains trailing spaces when values are
                        stored and retrieved while prior versions stripped the trailing spaces.
                        Any non HTML field should use STRIP_BOTH to prevent searching issues if the
                        database was orginally pre MySQL 5.0.3.
 Returns:      converted string.

=back

=cut

sub entity_replace_xml
{
	my ($string,$strip) = @_;
	
	return undef if ( ! defined $string );
	
	$string =~ s/^\s+// if ( $strip & STRIP_LEFT );
	$string =~ s/\s+$// if ( $strip & STRIP_RIGHT );
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
	my ($self, $xml, $filename) = @_;

	my $twig = XML::Twig->new( load_DTD => 1, keep_encoding => 1 );	

	if ( defined($xml) )
	{
		$twig->parse($xml);
	}
	elsif ( defined($filename) )
	{
		$twig->parsefile($filename);
	}
	else
	{
		$self->error("Bugzilla::Testopia::Xml::parse has no XML input source")
	}
	
	my $root = $twig->root;
	
	# Check for unimplemented tags.
	my @twig_builds = $root->children('build');
	$self->error("Support for <build> tags has not been implemented.") if ( $#twig_builds != -1 );
	my @twig_testenvironments = $root->children('testenvironment');
	$self->error("Support for <testenvironment> tags has not been implemented.") if ( $#twig_testenvironments != -1 );
	my @twig_testruns = $root->children('testrun');
	$self->error("Support for <testrun> tags has not been implemented.") if ( $#twig_testruns != -1 );
	my @twig_testrunlogs = $root->children('testrunlog');
	$self->error("Support for <testrunlog> tags has not been implemented.") if ( $#twig_testrunlogs != -1 );
	
	foreach my $twig_category ($root->children('category'))
	{
		my $category_name = entity_replace_xml($twig_category->field('name'),STRIP_BOTH);
		my $product_name = entity_replace_xml($twig_category->att('product'),STRIP_BOTH);
		my $description = entity_replace_xml($twig_category->field('description'),STRIP_BOTH);
		if ( $category_name eq "" )
		{
			$self->error("Category name cannot be empty, product='" . $product_name . "', description='" . $description . "'.");
			next;
		}
		
		$description = "FIX ME.  Created during category import with no description supplied." if ( $description eq "" );
		
		if ( $product_name eq REQUIRE )
		{
			$self->error("Must supply a product for category '" . $category_name . "'." );
			next;
		}
		
		my $product = new Bugzilla::Product({name => $product_name});
		if ( ! $product )
		{
			$self->error("Cannot find product '" . $product_name . "' for category '" . $category_name . "'.");
			$self->{"parser_error"} = 1;
			next;
		}
		
		my $category = new Bugzilla::Testopia::Category
		({
			name => $category_name,
			product_id => $product->id(),
			description => $description,
		});
		
		# Only create the category if it does not exist.
		push @{$self->categories}, $category if ( ! $category->check_name($category_name) );
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
		
		my $name = entity_replace_xml($twig_testplan->field('name'),STRIP_BOTH) || undef;
		$self->error("Found empty Test Plan name.") if ( ! defined($name) );
		$self->error("Length of Test Plan name '" . $name . "' must be " . Bugzilla::Testopia::TestPlan->NAME_MAX_LENGTH . " characters or less.") if ( defined($name) && ( length($name) > Bugzilla::Testopia::TestPlan->NAME_MAX_LENGTH ) );
		
		$testplan = Bugzilla::Testopia::TestPlan->new({
            'name'						=> $name,
            'product_id'				=> $product_id,
            'default_product_version'	=> entity_replace_xml($twig_testplan->field('productversion'),STRIP_BOTH),
            'type_id'					=> $plantype_ids{$twig_testplan->att('type')},
            'text'						=> entity_replace_testopia($twig_testplan->field('document')),
            'author_id'					=> $author_id,
			'isactive'					=> entity_replace_xml($twig_testplan->field('archive'),STRIP_BOTH),
			'creation_date'				=> entity_replace_xml($twig_testplan->field('created'),STRIP_BOTH)
    	});
		push @{$self->testplans}, $testplan;
		
		my @tags = $twig_testplan->children('tag');
		foreach my $twig_tag (@tags)
		{
			push @{$self->tags}, entity_replace_xml($twig_tag->text(),STRIP_BOTH);
		}
		
		my @attachments = $twig_testplan->children('attachment');
		foreach my $twig_attachments (@attachments)
		{
			my $submitter = $twig_attachments->field('submitter');
			# Bugzilla::User::match returns a array with a user hash.  Fields of the hash needed
			# are 'id' and 'login'.
			my $submitter_ref = Bugzilla::User::match($submitter, 1, 0);
			my $submitter_id = -1;
			if ( ! $submitter_ref->[0] )
			{
				$self->error("Cannot find submitter '" . $submitter . "' in test plan '" . $twig_testplan->field('name') . "' attachment '" . $twig_attachments->field('description') . "'.");
			}
			else
			{
				my $submitter_user = $submitter_ref->[0];
				bless($submitter_user,"Bugzilla::User");
				$submitter_id = $submitter_user->id();
			}
			my $attachment = Bugzilla::Testopia::Attachment->new({
            	'description'  => entity_replace_xml($twig_attachments->field('description'),STRIP_BOTH),
            	'filename'     => entity_replace_xml($twig_attachments->field('filename'),STRIP_BOTH),
            	'submitter_id' => $submitter_id,
            	'mime_type'    => entity_replace_xml($twig_attachments->field('mimetype'),STRIP_BOTH),
            	'contents'      => entity_replace_xml($twig_attachments->field('data'),STRIP_BOTH)
    		});
			push @{$self->attachments}, $attachment;
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
		my $summary = entity_replace_xml($twig_testcase->field('summary'),STRIP_BOTH) || undef;
		$self->error("Found empty Test Case summary.") if ( ! defined($summary) );
		$self->error("Length of summary '" . $summary . "' must be " . Bugzilla::Testopia::TestCase->SUMMARY_MAX_LENGTH . " characters or less.") if ( defined($summary) && ( length($summary) > Bugzilla::Testopia::TestCase->SUMMARY_MAX_LENGTH ) );
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
		my $tester = entity_replace_xml($twig_testcase->field('defaulttester'),STRIP_BOTH);
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
		my $alias = entity_replace_xml($twig_testcase->field('alias'),STRIP_BOTH) || undef;
		$self->error("Length of alias '" . $alias . "' in test case '" . $summary . "' must be " . Bugzilla::Testopia::TestCase->ALIAS_MAX_LENGTH . " characters or less.") if ( defined($alias) && ( length($alias) > Bugzilla::Testopia::TestCase->ALIAS_MAX_LENGTH ) );
		my $requirement = entity_replace_xml($twig_testcase->field('requirement'),STRIP_BOTH) || undef;
		$self->error("Length of requirement '" . $requirement . "' in test case '" . $summary . "' must be " . Bugzilla::Testopia::TestCase->REQUIREMENT_MAX_LENGTH . " characters or less.") if ( defined($requirement) && ( length($requirement) > Bugzilla::Testopia::TestCase->REQUIREMENT_MAX_LENGTH ) );
		
		$xml_testcase->testcase(Bugzilla::Testopia::TestCase->new({
			'action'			=> entity_replace_testopia($twig_testcase->field('action')),
            'alias'          	=> $alias,
            'arguments'      	=> entity_replace_xml($twig_testcase->field('arguments'),STRIP_NONE),
            'author_id'  		=> $author_id,
            'blocks'     		=> undef,
            'breakdown'         => entity_replace_testopia($twig_testcase->field('breakdown')),
            'case_status_id' 	=> $status_id,
            'category_id'    	=> undef,
            'default_tester_id' => $tester_id,
            'dependson'  		=> undef,
            'effect'			=> entity_replace_testopia($twig_testcase->field('expectedresults')),
            'isautomated'    	=> ( uc $twig_testcase->att('automated') ) eq AUTOMATIC,
            'plans'      		=> undef,
            'priority_id'    	=> $priority_ids{$twig_testcase->att('priority')},
            'requirement'    	=> $requirement,
            'setup'         	=> entity_replace_testopia($twig_testcase->field('setup')),
            'script'         	=> entity_replace_xml($twig_testcase->field('script'),STRIP_NONE),
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
    		elsif ( length($testplan_reference) > Bugzilla::Testopia::TestPlan->NAME_MAX_LENGTH )
    		{
    			$self->error("Length of Test Plan name '" . $testplan_reference . "' for test case '" . $summary . "' must be " . Bugzilla::Testopia::TestCase->REQUIREMENT_MAX_LENGTH . " characters or less.");
    		}
	   		elsif ( ! $xml_testcase->testplan->add($twig_testplan_reference->att('type'),entity_replace_xml($testplan_reference,STRIP_BOTH)) )
    		{
    			$self->error("Do not know how to handle test plan of type '" 
    			             . $twig_testplan_reference->att('type') 
    			             . "' in test case '" . $twig_testcase->field('summary') . "'." 
    			             . "\nKnow types are: (" . uc XMLREFERENCES_FIELDS . ").");
    		}
    	}
    	# Keep track of this testcase's alias.  Used during verification to insure aliases are unique.
    	$self->testcase_aliases(entity_replace_xml($twig_testcase->field('summary'),STRIP_BOTH),$alias) if ( defined $alias );
    	# Keep track of this testcase's category.  To create a category at this time would require
    	# getting the product from the Test Plan that this Test Case is associated with.  The category
    	# will created when each Test Case is stored.
    	my $categoryname = entity_replace_xml($twig_testcase->field('categoryname'),STRIP_BOTH);
    	if ( $categoryname ne "" )
    	{
     		$xml_testcase->category($categoryname);
    	}
    	else
    	{
    		$self->error("Empty category name for test case '" . $summary . "'.");
    	}
 
 		my @attachments = $twig_testcase->children('attachment');
		foreach my $twig_attachments (@attachments)
		{
			my $submitter = $twig_attachments->field('submitter');
			# Bugzilla::User::match returns a array with a user hash.  Fields of the hash needed
			# are 'id' and 'login'.
			my $submitter_ref = Bugzilla::User::match($submitter, 1, 0);
			my $submitter_id = -1;
			if ( ! $submitter_ref->[0] )
			{
				$self->error("Cannot find submitter '" . $submitter . "' in test case '" . $twig_testcase->field('summary') . "' attachment '" . $twig_attachments->field('description') . "'.");
			}
			else
			{
				my $submitter_user = $submitter_ref->[0];
				bless($submitter_user,"Bugzilla::User");
				$submitter_id = $submitter_user->id();
			}
			my $attachment = Bugzilla::Testopia::Attachment->new({
            	'description'  => entity_replace_xml($twig_attachments->field('description'),STRIP_BOTH),
            	'filename'     => entity_replace_xml($twig_attachments->field('filename'),STRIP_BOTH),
            	'submitter_id' => $submitter_id,
            	'mime_type'    => entity_replace_xml($twig_attachments->field('mimetype'),STRIP_BOTH),
            	'contents'      => entity_replace_xml($twig_attachments->field('data'),STRIP_BOTH)
    		});
			$xml_testcase->add_attachment($attachment);
		}
		
    	my @tags = $twig_testcase->children('tag');
		foreach my $twig_tag ( @tags )
		{
			my $tag = entity_replace_xml($twig_tag->text(),STRIP_BOTH);
			$self->error("Length of tag '" . $tag . "' in test case '" . $summary . "' must be " . Bugzilla::Testopia::TestCase->TAG_MAX_LENGTH . " characters or less.") if ( defined($tag) && ( length($tag) > Bugzilla::Testopia::TestCase->TAG_MAX_LENGTH ) );
			$xml_testcase->add_tag($tag);
		}
    
    	my @components = $twig_testcase->children('component');
    	foreach my $twig_component ( @components )
		{
			my $results = $xml_testcase->add_component(entity_replace_xml($twig_component->children_text(PCDATA),STRIP_BOTH),$twig_component->att('product'));
			$self->error($results) if ( $results ne "" );
    	}
    	
    	foreach my $twig_blocks ( $twig_testcase->children(BLOCKS) )
    	{
	   		if ( ! $xml_testcase->blocks->add($twig_blocks->att('type'),entity_replace_xml($twig_blocks->children_text(PCDATA),STRIP_BOTH)) )
    		{
    			$self->error("Do not know how to handle a blocking test case of type '" . $twig_blocks->att('type') . "' in test case '" . $xml_testcase->testcase->summary() . "'.") 
    		}
    	}

    	foreach my $twig_dependson ( $twig_testcase->children(DEPENDSON) )
    	{
	   		if ( ! $xml_testcase->dependson->add($twig_dependson->att('type'),entity_replace_xml($twig_dependson->children_text(PCDATA),STRIP_BOTH)) )
    		{
    			$self->error("Do not know how to handle dependency of type '" . $twig_dependson->att('type') . "' in test case '" . entity_replace_xml($xml_testcase->testcase->summary(),STRIP_BOTH) . "'.") 
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
		# Store new categories.
		foreach my $category ( @{$self->categories} )
		{
			# Make sure category still does not exist.  We don't check for uniqueness above so
			# the same category could be defined multiple times.
			if ( ! $category->check_name($category->name()) )
			{
				$category->store();
				my $product_name = Bugzilla::Testopia::Product->new($category->product_id())->name();
				print "Created category '" . $category->name() . "': " . $category->description() . " for product " . $product_name . ".\n";
			}
		}
		
		# Store new testplans.
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
			foreach my $attachment ( @{$self->attachments} )
			{
				$attachment->{'plan_id'} = $plan_id;
				$attachment->store();
			}
			print "Created Test Plan $plan_id: " . $testplan->name() . "\n";
		}
		
		# Store new testcases.
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

=head1 SEE ALSO

Testopia::(TestPlan, TestCase, TestRun, Category, Build, Environment)

=head1 AUTHOR

David Koenig <dkoenig@novell.com>

=cut

1;
