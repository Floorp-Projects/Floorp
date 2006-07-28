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
		action			=> '$',
		blocks			=> '@',
		category		=> '$',
		dependson		=> '@',
		expectedresults	=> '$',
		tags			=> '@',
		testcase		=> 'Bugzilla::Testopia::TestCase',
	}
);

sub debug_display()
{	
	my ($self) = @_;
	my $display_variable = "";

	print STDERR "Testcase: " . $self->testcase->summary() . "\n";
		my $testcase_id = "null";
		$testcase_id = $self->testcase->id() if ( $self->testcase->id() );
		print STDERR "   ID                      " . $testcase_id . "\n";
		print STDERR "   Action\n";
		my @results = split(/\n/,$self->action);
		foreach my $result (@results)
		{
			print STDERR "      $result\n";
		}
	   	my $alias = "null";
		$alias = $self->testcase-alias() if ( $self->testcase->alias() );
		print STDERR "   Alias                   " . $alias . "\n";
		my %author = %{$self->testcase->author()};
		my $author_id = $author{"id"};
		my $author_login = $author{"login"};
		print STDERR "   Author                  " . $author_login . " (id=" . $author_id . ", hash=" . $self->testcase->author() . ")\n";
		foreach my $loop ( @{$self->blocks} )
		{
			$display_variable .= $loop . ",";
		}
		chop $display_variable;
		print STDERR "   Blocks                  " . $display_variable . "\n";
		$display_variable = "";
		print STDERR "   Category                " . $self->category . "\n";
		print STDERR "   Creation Date           " . $self->testcase->creation_date() . "\n";
				foreach my $loop ( @{$self->blocks} )
		{
			$display_variable .= $loop . ",";
		}
		chop $display_variable;
		print STDERR "   Dependson               " . $display_variable . "\n";
		$display_variable = "";
		print STDERR "   Expected Results\n";
		@results = split(/\n/,$self->expectedresults);
		foreach my $result (@results)
		{
			print STDERR "      $result\n";
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
			my $author_login = $author{"login"};
			print STDERR "   Attachment                     " . $attachment->description() . "\n";
			print STDERR "      Creation Date               " . $attachment->creation_ts(). "\n";
			print STDERR "      Filename                    " . $attachment->filename() . "\n";
			print STDERR "      Mime Type                   " . $attachment->mime_type(). "\n";
			print STDERR "      Submitter                   " . $author_login . "\n";
		}
}

sub add_tag()
{
		my ($self,$tag) = @_;
		
		push @{$self->tags}, $tag;
}

sub store()
{
	my ($self, $userid, @testplans) = @_;
	
	#
	# Following code pulled from Bugzilla::Testopia::TestCase->get_category_list().
	# Cannot call Bugzilla::Testopia::TestCase->get_category_list() until we create a 
	# TestCase but cannot create a TestCase without a category.  
	#
	my @categories;
	foreach my $testplan (@testplans)
	{
		push @categories, @{$testplan->categories};
    }
    my $categoryid = -1;
    foreach my $category (@categories)
    {
    	my %temphash = %{$category};
		if ( $self->category eq $temphash{"name"} )
		{
			$categoryid = $temphash{"category_id"};
			last;
		}
    }
    return "Cannot save test case '" . $self->testcase->summary . "', Category '" . $self->category . "' is not valid." if ( $categoryid == -1 );
	$self->testcase->{'category_id'} = $categoryid;
	
	my $case_id = $self->testcase->store();
	$self->testcase->{'case_id'} = $case_id;
	foreach my $asciitag ( @{$self->tags} )
	{
		my $classtag = Bugzilla::Testopia::TestTag->new({'tag_name' => $asciitag});
		my $tagid = $classtag->store;
		$self->testcase->add_tag($tagid);
	}
	foreach my $testplan ( @testplans )
	{
		$self->testcase->link_plan($testplan->id(),$case_id)
	}
	$self->testcase->store_text($case_id,$userid,$self->action,$self->expectedresults);
	
	return "";
}

1;
