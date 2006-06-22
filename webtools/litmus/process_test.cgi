#!/usr/bin/perl -w
# -*- mode: cperl; c-basic-offset: 8; indent-tabs-mode: nil; -*-

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Litmus.
#
# The Initial Developer of the Original Code is
# the Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chris Cooper <ccooper@deadsquid.com>
#   Zach Lipton <zach@zachlipton.com>
#
# ***** END LICENSE BLOCK *****

use strict;

use Litmus;
use Litmus::Error;
use Litmus::DB::Product;
use Litmus::UserAgentDetect;
use Litmus::SysConfig;
use Litmus::Auth;
use Litmus::Utils;
use Litmus::DB::Resultbug;
use Litmus::XML;

use CGI;
use Date::Manip;
use diagnostics;

my $c = Litmus->cgi(); 

if ($c->param('data')) {
	# we're getting XML result data from an automated testing provider, 
	# so pass that off to XML.pm for processing
	my $x = Litmus::XML->new();
	$x->processResults($c->param('data'));
	
	# return whatever response was generated:
	print $c->header('text/plain');
	print $x->response();
	exit; # that's all folks!
}

my $user;
my $sysconfig;
if ($c->param("isSysConfig")) {
  $sysconfig = Litmus::SysConfig->processForm($c);
  my $email = $c->param("email");
  $user = Litmus::DB::User->find_or_create(email => $email);
  print $c->header(-cookie => [$sysconfig->setCookie(), Litmus::Auth::setCookie($user)]);
} else {
  print $c->header();
}

my @names = $c->param();

# find all the test numbers contained in this result submission
my @tests;
foreach my $curname (@names) {
  if ($curname =~ /testresult_(\d*)/) {
    push(@tests, $1);
  }
}

# don't get to use the simple test interface if you really 
# have more than one test (i.e. you cheated and changed the 
# hidden input)
if (scalar @tests > 1 && $c->param("isSimpleTest")) {
  invalidInputError("Cannot use simpletest interface with more than one test");
}

my $testcount;
my %resultcounts;
my $product;
foreach my $curtestid (@tests) {
  unless ($c->param("testresult_".$curtestid)) {
    # user didn't submit a result for this test so just skip 
    # it and move on...
    next; 
  }
  
  my $curtest = Litmus::DB::Testcase->retrieve($curtestid);
  unless ($curtest) {
    # oddly enough, the test doesn't exist
    next;
  }
  
  $testcount++;
  
  $product = $curtest->product();
  
  my $ua = Litmus::UserAgentDetect->new();
  # for simpletest, build a temporary sysconfig based on the 
  # UA string and product of this test:
  if ($c->param("isSimpleTest")) {
    $sysconfig = Litmus::SysConfig->new(
                                        $curtest->product(), 
                                        $ua->platform($curtest->product()), 
                                        "NULL", # no way to autodetect the opsys
                                        $ua->branch($curtest->product()), 
                                        $ua->build_id(),
                                       );
  }
  
  # get system configuration. If there is no configuration and we're 
  # not doing the simpletest interface, then we make you enter it
  # Get system configuration. If there is no configuration,
  # then we make the user enter it.
  if (!$sysconfig) {
    $sysconfig = Litmus::SysConfig->getCookie($product);
  }
  
  # Users who still don't have a sysconfig for this product
  # should go configure themselves first.
  if (!$sysconfig) {
    Litmus::SysConfig->displayForm($product,
                                   "process_test_results.cgi",
                                   $c);
    exit;
  }
  
  my $result_status = Litmus::DB::ResultStatus->retrieve($c->param("testresult_".$curtestid));
  $resultcounts{$result_status->name()}++;
  
  my $note = $c->param("comment_".$curtestid);
  my $bugs = $c->param("bugs_".$curtestid);
  
  my $time = &Date::Manip::UnixDate("now","%q");
  
  # normally, the user comes with a cookie, but for simpletest
  # users, we just use the web-user@mozilla.org user:
  
  if ($c->param("isSimpleTest")) {
    $user = $user || Litmus::DB::User->search(email => 'web-tester@mozilla.org')->next();
  } else {
    $user = $user || Litmus::Auth::getCookie()->user_id();
  } 
  
  my $tr = Litmus::DB::Testresult->create({
                                           user      => $user,
                                           testcase    => $curtest,
                                           timestamp => $time,
                                           last_updated => $time,
                                           useragent => $ua,
                                           result_status => $result_status,
                                           opsys     => $sysconfig->opsys(),
                                           branch    => $sysconfig->branch(),
                                           build_id   => $sysconfig->build_id(),
                                           locale_abbrev => $sysconfig->locale(),
                                          });
  
  # if there's a note, create an entry in the comments table for it
  if ($note and
      $note ne '') {
    Litmus::DB::Comment->create({
                                 testresult      => $tr,
                                 submission_time => $time,
                                 last_updated    => $time,
                                 user            => $user,
                                 comment         => $note
                                });
  }
  
  if ($bugs and
      $bugs ne '') {
    $bugs =~ s/[^0-9,]//g;
    my @new_bugs = split(/,/,$bugs);
    foreach my $new_bug (@new_bugs) {
      next if ($new_bug eq '0');
      my $bug = Litmus::DB::Resultbug->create({
                                               testresult => $tr,
                                               last_updated => $time,
                                               submission_time => $time,
                                               user => $user,
                                               bug_id => $new_bug,
                                              });
    }
  }

}

if (! $testcount) {
  invalidInputError("No results submitted.");
}


  
my $testgroup;
if ($c->param("testgroup")) {
  $testgroup = Litmus::DB::Testgroup->retrieve($c->param("testgroup"));
  if (! $product) {
    $product = $testgroup->product();
  }
} 

my $vars;
$vars->{'title'} = 'Run Tests';

my $cookie =  Litmus::Auth::getCookie();
$vars->{"defaultemail"} = $cookie;
$vars->{"show_admin"} = Litmus::Auth::istrusted($cookie);

$vars->{'testcount'} = $testcount;
$vars->{'product'} = $product || undef;
$vars->{'resultcounts'} = \%resultcounts || undef;
$vars->{'testgroup'} = $testgroup || undef;
$vars->{'return'} = $c->param("return") || undef;

Litmus->template()->process("process/process.html.tmpl", $vars) ||
  internalError(Litmus->template()->error());    

