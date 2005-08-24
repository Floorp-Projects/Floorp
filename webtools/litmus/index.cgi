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

# Litmus homepage

use Litmus;
use Litmus::Error;
use Litmus::DB::Product;
use Litmus::Testlist;
use CGI;

my $c = new CGI; 

print $c->header();

my @products = Litmus::DB::Product->retrieve_all();

my @failingtests = Litmus::DB::Test->retrieve_all();
#my @hotlist = makeHotlist(15, Litmus::DB::Result->search(name => "Fail"), @failingtests);
my @hotlist = undef;


my $vars = {
    products => \@products,
    hotlist  => \@hotlist,
};

Litmus->template()->process("index.html.tmpl", $vars) || 
    internalError(Litmus->template()->error());