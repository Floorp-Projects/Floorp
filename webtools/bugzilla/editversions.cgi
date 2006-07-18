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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Holger
# Schurig. Portions created by Holger Schurig are
# Copyright (C) 1999 Holger Schurig. All
# Rights Reserved.
#
# Contributor(s): Holger Schurig <holgerschurig@nikocity.de>
#                 Terry Weissman <terry@mozilla.org>
#                 Gavin Shelley <bugzilla@chimpychompy.org>
#                 Frédéric Buclin <LpSolit@gmail.com>
#
#
# Direct any questions on this source code to
#
# Holger Schurig <holgerschurig@nikocity.de>

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Product;
use Bugzilla::Version;

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;
my $template = Bugzilla->template;
my $vars = {};

#
# Preliminary checks:
#

my $user = Bugzilla->login(LOGIN_REQUIRED);

print $cgi->header();

$user->in_group('editcomponents')
  || ThrowUserError("auth_failure", {group  => "editcomponents",
                                     action => "edit",
                                     object => "versions"});

#
# often used variables
#
my $product_name = trim($cgi->param('product') || '');
my $version_name = trim($cgi->param('version') || '');
my $action       = trim($cgi->param('action')  || '');
my $showbugcounts = (defined $cgi->param('showbugcounts'));

#
# product = '' -> Show nice list of products
#

unless ($product_name) {
    $vars->{'products'} = $user->get_selectable_products;
    $vars->{'showbugcounts'} = $showbugcounts;

    $template->process("admin/versions/select-product.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

# First make sure the product name is valid.
my $product = Bugzilla::Product::check_product($product_name);

# Then make sure the user is allowed to edit properties of this product.
$user->can_see_product($product->name)
  || ThrowUserError('product_access_denied', {product => $product->name});


#
# action='' -> Show nice list of versions
#

unless ($action) {
    $vars->{'showbugcounts'} = $showbugcounts;
    $vars->{'product'} = $product;
    $template->process("admin/versions/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}




#
# action='add' -> present form for parameters for new version
#
# (next action will be 'new')
#

if ($action eq 'add') {

    $vars->{'product'} = $product;
    $template->process("admin/versions/create.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='new' -> add version entered in the 'action=add' screen
#

if ($action eq 'new') {

    my $version = Bugzilla::Version::create($version_name, $product);

    $vars->{'version'} = $version;
    $vars->{'product'} = $product;
    $template->process("admin/versions/created.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}




#
# action='del' -> ask if user really wants to delete
#
# (next action would be 'delete')
#

if ($action eq 'del') {

    my $version = Bugzilla::Version::check_version($product, $version_name);

    $vars->{'version'} = $version;
    $vars->{'product'} = $product;
    $template->process("admin/versions/confirm-delete.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='delete' -> really delete the version
#

if ($action eq 'delete') {

    my $version = Bugzilla::Version::check_version($product, $version_name);
    $version->remove_from_db;

    $vars->{'version'} = $version;
    $vars->{'product'} = $product;

    $template->process("admin/versions/deleted.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='edit' -> present the edit version form
#
# (next action would be 'update')
#

if ($action eq 'edit') {

    my $version = Bugzilla::Version::check_version($product, $version_name);

    $vars->{'version'} = $version;
    $vars->{'product'} = $product;

    $template->process("admin/versions/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='update' -> update the version
#

if ($action eq 'update') {

    my $version_old_name = trim($cgi->param('versionold') || '');
    my $version =
        Bugzilla::Version::check_version($product, $version_old_name);

    $dbh->bz_lock_tables('bugs WRITE', 'versions WRITE');

    $vars->{'updated'} = $version->update($version_name, $product);

    $dbh->bz_unlock_tables();

    $vars->{'version'} = $version;
    $vars->{'product'} = $product;
    $template->process("admin/versions/updated.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# No valid action found
#
ThrowUserError('no_valid_action', {'field' => "version"});
