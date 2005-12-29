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

require "globals.pl";

use Bugzilla::Constants;
use Bugzilla::Config qw(:DEFAULT $datadir);
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
    $vars->{'product'} = $product->name;
    $vars->{'versions'} = $product->versions;
    $template->process("admin/versions/list.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}




#
# action='add' -> present form for parameters for new version
#
# (next action will be 'new')
#

if ($action eq 'add') {

    $vars->{'product'} = $product->name;
    $template->process("admin/versions/create.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='new' -> add version entered in the 'action=add' screen
#

if ($action eq 'new') {

    # Cleanups and valididy checks
    $version_name || ThrowUserError('version_blank_name');

    # Remove unprintable characters
    $version_name = clean_text($version_name);

    my $version = new Bugzilla::Version($product->id, $version_name);
    if ($version) {
        ThrowUserError('version_already_exists',
                       {'name' => $version->name,
                        'product' => $product->name});
    }

    # Add the new version
    trick_taint($version_name);
    $dbh->do("INSERT INTO versions (value, product_id)
              VALUES (?, ?)", undef, ($version_name, $product->id));

    # Make versioncache flush
    unlink "$datadir/versioncache";

    $vars->{'name'} = $version_name;
    $vars->{'product'} = $product->name;
    $template->process("admin/versions/created.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}




#
# action='del' -> ask if user really wants to delete
#
# (next action would be 'delete')
#

if ($action eq 'del') {

    my $version = Bugzilla::Version::check_version($product,
                                                   $version_name);
    my $bugs = $version->bug_count;

    $vars->{'bug_count'} = $bugs;
    $vars->{'name'} = $version->name;
    $vars->{'product'} = $product->name;
    $template->process("admin/versions/confirm-delete.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='delete' -> really delete the version
#

if ($action eq 'delete') {

    my $version = Bugzilla::Version::check_version($product,
                                                   $version_name);

    # The version cannot be removed if there are bugs
    # associated with it.
    if ($version->bug_count) {
        ThrowUserError("version_has_bugs",
                       { nb => $version->bug_count });
    }

    $dbh->do("DELETE FROM versions WHERE product_id = ? AND value = ?",
              undef, ($product->id, $version->name));

    unlink "$datadir/versioncache";

    $vars->{'name'} = $version->name;
    $vars->{'product'} = $product->name;

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

    my $version = Bugzilla::Version::check_version($product,
                                                   $version_name);

    $vars->{'name'}    = $version->name;
    $vars->{'product'} = $product->name;

    $template->process("admin/versions/edit.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='update' -> update the version
#

if ($action eq 'update') {

    $version_name || ThrowUserError('version_not_specified');

    # Remove unprintable characters
    $version_name = clean_text($version_name);

    my $version_old_name = trim($cgi->param('versionold') || '');
    my $version_old =
        Bugzilla::Version::check_version($product,
                                         $version_old_name);

    # Note that the order of this tests is important. If you change
    # them, be sure to test for WHERE='$version' or WHERE='$versionold'

    $dbh->bz_lock_tables('bugs WRITE',
                         'versions WRITE',
                         'products READ');

    if ($version_name ne $version_old->name) {
        
        my $version = new Bugzilla::Version($product->id,
                                            $version_name);

        if ($version) {
            ThrowUserError('version_already_exists',
                           {'name' => $version->name,
                            'product' => $product->name});
        }
        
        trick_taint($version_name);
        $dbh->do("UPDATE bugs
                  SET version = ?
                  WHERE version = ? AND product_id = ?", undef,
                  ($version_name, $version_old->name, $product->id));
        
        $dbh->do("UPDATE versions
                  SET value = ?
                  WHERE product_id = ? AND value = ?", undef,
                  ($version_name, $product->id, $version_old->name));

        unlink "$datadir/versioncache";

        $vars->{'updated_name'} = 1;
    }

    $dbh->bz_unlock_tables(); 

    $vars->{'name'} = $version_name;
    $vars->{'product'} = $product->name;
    $template->process("admin/versions/updated.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# No valid action found
#
ThrowUserError('no_valid_action', {'field' => "version"});
