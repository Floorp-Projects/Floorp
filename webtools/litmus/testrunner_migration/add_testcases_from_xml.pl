#!/usr/bin/perl -w
# -*- mode: cperl; c-basic-offset: 8; indent-tabs-mode: nil; -*-
use strict;
$|++;

use XML::XPath;
use XML::XPath::XMLParser;
use Data::Dumper;
use Date::Manip;
use Getopt::Long;

use lib qw(..);

use Litmus;

my $file;
my $verbose;
GetOptions (
	    "file=s"  => \$file,
	    "verbose" => \$verbose
	   );

if (!$file) {
  die "File not specified." 
}

if (! -e $file) {
  die "File $file not found." 
}

my $xp = XML::XPath->new(filename => $file);

# Get all testgroups.
my $testgroups = $xp->find('/litmus/testgroups/testgroup');

foreach my $testgroup_node ($testgroups->get_nodelist) {
  my $testgroup_name = $testgroup_node->findvalue('/litmus/testgroups/testgroup/name');
  next if (!$testgroup_name);

  if ($verbose) {
    print "Looking for testgroup: $testgroup_name...\n";
  }
  
  # Check whether testgroup already exists. Add it if not.
  my ($testgroup) = Litmus::DB::Testgroup->search(name => "$testgroup_name");
  if (!$testgroup) {
    if ($verbose) {
      print "Testgroup: $testgroup_name not found. Attempting to add...";
    }
    my $product_name = $testgroup_node->findvalue('product');
    my ($product) = Litmus::DB::Product->search(name => "$product_name");
    die if (!$product);
    my %hash = (
		name => $testgroup_name,
		product_id => $product->product_id,
	       );
    $testgroup = Litmus::DB::Testgroup->create(\%hash);
    die if (!$testgroup);
    print "Added!\n";
  } else {
    if ($verbose) {
      print "Found existing testgroup: $testgroup_name\n";
    }
  }
  my $branch_name = $testgroup_node->findvalue('branch'); 
  my ($branch) = Litmus::DB::Branch->search(name => "$branch_name");
  die if (!$branch);
  $testgroup->add_branch($branch->branch_id);
  
  # Get a list of all subgroups for the current testgroup.
  my $subgroups = $testgroup_node->find('subgroups/subgroup');
  
  my $sg_default_order = 1;
  foreach my $subgroup_node ($subgroups->get_nodelist) {
    my $subgroup_name = $subgroup_node->findvalue('name');
    next if (!$subgroup_name);

    if ($verbose) {
      print "Looking for subgroup: $subgroup_name...\n";
    }
    
    # Check whether subgroup already exists. Add it if not.
    my ($subgroup) = Litmus::DB::Subgroup->search(name => "$subgroup_name");
    if (!$subgroup) {
      if ($verbose) {
	print "Subgroup: $subgroup_name not found. Attempting to add...";
      }
      my $product_name = $subgroup_node->findvalue('product');
      my ($product) = Litmus::DB::Product->search(name => "$product_name");
      die if (!$product);
      my %hash = (
		  name => $subgroup_name,
		  product_id => $product->product_id,
		 );
      $subgroup = Litmus::DB::Subgroup->create(\%hash);
      die if (!$subgroup);
      print "Added!\n";
    } else {
      if ($verbose) {
	print "Found existing subgroup: $subgroup_name\n";
      }
    }
    
    my $sg_sort_order = $subgroup_node->findvalue('sort_order');
    if (!$sg_sort_order) {
	$sg_sort_order = $sg_default_order;
    }
    $subgroup->update_testgroup($testgroup->testgroup_id,$sg_sort_order);
    $sg_default_order++;
    
    # Get all testcases for the current subgroup.
    my $testcases = $subgroup_node->find('testcases/testcase');
    
    my $tc_default_order = 1;
    foreach my $testcase_node ($testcases->get_nodelist) {
      my $testcase_name = $testcase_node->findvalue('name');
      next if (!$testcase_name);
      
      # We assume that all testcases are new.
      if ($verbose) {
	print "Attempting to add testcase: $testcase_name...";
      }
      my $product_name = $testcase_node->findvalue('product');
      my ($product) = Litmus::DB::Product->search(name => "$product_name");
      die if (!$product);
      my $email = $testcase_node->findvalue('author_email');
      my ($user) = Litmus::DB::User->search(email => "$email");
      
      my $steps = $testcase_node->find('steps/*');
      my $steps_text;
      foreach my $step ($steps->get_nodelist) {
	$steps_text .= XML::XPath::XMLParser::as_string($step);
      }
      my $results = $testcase_node->find('expected_results/*');
      my $results_text;
      foreach my $result ($results->get_nodelist) {
	$results_text .= XML::XPath::XMLParser::as_string($result);
      }
      
      my %hash = (
		  summary => $testcase_name,
		  product_id => $product->product_id,
		  author_id => $user->user_id || 0,
		  details => $testcase_node->findvalue('details'),
		  steps => $steps_text,
		  expected_results => $results_text,
		  format => 1,
		 );

      my $testcase = Litmus::DB::Testcase->create(\%hash);
      die if (!$testcase);	    
      print "Added!\n";
      my $tc_sort_order = $testcase_node->findvalue('sort_order');
      if (!$tc_sort_order) {
	$tc_sort_order = $tc_default_order;
      }
      $testcase->update_subgroup($subgroup->subgroup_id,$tc_sort_order);
      $tc_default_order++;
    }

  }    

}
