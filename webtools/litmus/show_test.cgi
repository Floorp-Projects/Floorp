#!/usr/bin/perl -w
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
# The Original Code is Litmus.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Zach Lipton <zach@zachlipton.com>

use strict;

use Litmus;
use Litmus::Error;
use Litmus::DB::Product;
use Litmus::UserAgentDetect;
use Litmus::SysConfig;
use Litmus::Auth;

use CGI;
use Time::Piece::MySQL;

my $c = new CGI; 

print $c->header();

my $testid = $c->param("id");

my $vars;
my $cookie =  Litmus::Auth::getCookie();
$vars->{"defaultemail"} = $cookie;
$vars->{"show_admin"} = Litmus::Auth::istrusted($cookie);

if (! $testid) {
    Litmus->template()->process("show/enter_id.html.tmpl", $vars) || 
        internalError(Litmus->template()->error());
    exit;
} 

my $test = Litmus::DB::Test->retrieve($testid);

if (! $test) {
    invalidInputError("Test $testid is not a valid test id");
}

my @results = Litmus::DB::Result->retrieve_all();

my $showallresults = $c->param("showallresults") || "";

my @where;
push @where, { field => 'test_id', value => $testid };
my @order_by;
push @order_by, { field => 'created', direction => 'DESC' };
my $test_results = Litmus::DB::Testresult->getTestResults(\@where,\@order_by);

$vars = {
    test => $test,
    results => \@results,
    showallresults => $showallresults,
    test_results => $test_results,    
    defaultemail => $cookie,
    show_admin => Litmus::Auth::istrusted($cookie),
};

Litmus->template()->process("show/show.html.tmpl", $vars) || 
        internalError(Litmus->template()->error());    
