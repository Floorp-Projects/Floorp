#!/usr/bin/perl -wT
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
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>

use vars qw(
  %legal_product
);

use strict;

use lib qw(.);

use Bugzilla;
use Bugzilla::Constants;

require "CGI.pl";

Bugzilla->login();

GetVersionTable();

my $cgi = Bugzilla->cgi;
my $product = $cgi->param('product');

if (!defined $product) {
    # Reference to a subset of %::proddesc, which the user is allowed to see
    my %products;

    if (AnyEntryGroups()) {
        # OK, now only add products the user can see
        Bugzilla->login(LOGIN_REQUIRED);
        foreach my $p (@::legal_product) {
            if (CanEnterProduct($p)) {
                $products{$p} = $::proddesc{$p};
            }
        }
    }
    else {
          %products = %::proddesc;
    }

    my $prodsize = scalar(keys %products);
    if ($prodsize == 0) {
        ThrowUserError("no_products");
    }
    elsif ($prodsize > 1) {
        $::vars->{'proddesc'} = \%products;
        $::vars->{'target'} = "describecomponents.cgi";

        print $cgi->header();
        $::template->process("global/choose-product.html.tmpl", $::vars)
          || ThrowTemplateError($::template->error());
        exit;
    }

    $product = (keys %products)[0];
}

# Make sure the user specified a valid product name.  Note that
# if the user specifies a valid product name but is not authorized
# to access that product, they will receive a different error message
# which could enable people guessing product names to determine
# whether or not certain products exist in Bugzilla, even if they
# cannot get any other information about that product.
my $product_id = get_product_id($product);

if (!$product_id) {
    ThrowUserError("invalid_product_name",
                   { product => $product });
}

# Make sure the user is authorized to access this product.
CanEnterProduct($product)
      || ThrowUserError("product_access_denied");

######################################################################
# End Data/Security Validation
######################################################################

my @components;
SendSQL("SELECT name, initialowner, initialqacontact, description FROM " .
        "components WHERE product_id = $product_id ORDER BY " .
        "name");
while (MoreSQLData()) {
    my ($name, $initialowner, $initialqacontact, $description) =
      FetchSQLData();

    my %component;

    $component{'name'} = $name;
    $component{'initialowner'} = $initialowner ?
      DBID_to_name($initialowner) : '';
    $component{'initialqacontact'} = $initialqacontact ?
      DBID_to_name($initialqacontact) : '';
    $component{'description'} = $description;

    push @components, \%component;
}

$::vars->{'product'} = $product;
$::vars->{'components'} = \@components;

print $cgi->header();
$::template->process("reports/components.html.tmpl", $::vars)
  || ThrowTemplateError($::template->error());

