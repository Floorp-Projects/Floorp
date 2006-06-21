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
#                 Frédéric Buclin <LpSolit@gmail.com>
#
# Direct any questions on this source code to
#
# Holger Schurig <holgerschurig@nikocity.de>

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Config qw(:DEFAULT);
use Bugzilla::Series;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::User;
use Bugzilla::Product;
use Bugzilla::Component;
use Bugzilla::Bug;

my $cgi = Bugzilla->cgi;
my $dbh = Bugzilla->dbh;
my $template = Bugzilla->template;
my $vars = {};

#
# Preliminary checks:
#

my $user = Bugzilla->login(LOGIN_REQUIRED);
my $whoid = $user->id;

print $cgi->header();

$user->in_group('editcomponents')
  || ThrowUserError("auth_failure", {group  => "editcomponents",
                                     action => "edit",
                                     object => "components"});

#
# often used variables
#
my $product_name  = trim($cgi->param('product')     || '');
my $comp_name     = trim($cgi->param('component')   || '');
my $action        = trim($cgi->param('action')      || '');
my $showbugcounts = (defined $cgi->param('showbugcounts'));

#
# product = '' -> Show nice list of products
#

unless ($product_name) {
    $vars->{'products'} = $user->get_selectable_products;
    $vars->{'showbugcounts'} = $showbugcounts;

    $template->process("admin/components/select-product.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

# First make sure the product name is valid.
my $product = Bugzilla::Product::check_product($product_name);

# Then make sure the user is allowed to edit properties of this product.
$user->can_see_product($product->name)
  || ThrowUserError('product_access_denied', {product => $product->name});


#
# action='' -> Show nice list of components
#

unless ($action) {

    $vars->{'showbugcounts'} = $showbugcounts;
    $vars->{'product'} = $product;
    $template->process("admin/components/list.html.tmpl", $vars)
        || ThrowTemplateError($template->error());

    exit;
}


#
# action='add' -> present form for parameters for new component
#
# (next action will be 'new')
#

if ($action eq 'add') {

    $vars->{'product'} = $product;
    $template->process("admin/components/create.html.tmpl", $vars)
        || ThrowTemplateError($template->error());

    exit;
}



#
# action='new' -> add component entered in the 'action=add' screen
#

if ($action eq 'new') {
    
    # Do the user matching
    Bugzilla::User::match_field ($cgi, {
        'initialowner'     => { 'type' => 'single' },
        'initialqacontact' => { 'type' => 'single' },
    });

    my $default_assignee   = trim($cgi->param('initialowner')     || '');
    my $default_qa_contact = trim($cgi->param('initialqacontact') || '');
    my $description        = trim($cgi->param('description')      || '');

    $comp_name || ThrowUserError('component_blank_name');

    if (length($comp_name) > 64) {
        ThrowUserError('component_name_too_long',
                       {'name' => $comp_name});
    }

    my $component =
        new Bugzilla::Component({product_id => $product->id,
                                 name => $comp_name});

    if ($component) {
        ThrowUserError('component_already_exists',
                       {'name' => $component->name});
    }

    $description || ThrowUserError('component_blank_description',
                                   {name => $comp_name});

    $default_assignee || ThrowUserError('component_need_initialowner',
                                        {name => $comp_name});

    my $default_assignee_id   = login_to_id($default_assignee);
    my $default_qa_contact_id = Param('useqacontact') ?
        (login_to_id($default_qa_contact) || undef) : undef;

    trick_taint($comp_name);
    trick_taint($description);

    $dbh->do("INSERT INTO components
                (product_id, name, description, initialowner,
                 initialqacontact)
              VALUES (?, ?, ?, ?, ?)", undef,
             ($product->id, $comp_name, $description,
              $default_assignee_id, $default_qa_contact_id));

    # Insert default charting queries for this product.
    # If they aren't using charting, this won't do any harm.
    my @series;

    my $prodcomp = "&product="   . url_quote($product->name) .
                   "&component=" . url_quote($comp_name);

    # For localisation reasons, we get the title of the queries from the
    # submitted form.
    my $open_name = $cgi->param('open_name');
    my $nonopen_name = $cgi->param('nonopen_name');
    my $open_query = "field0-0-0=resolution&type0-0-0=notregexp&value0-0-0=." .
                     $prodcomp;
    my $nonopen_query = "field0-0-0=resolution&type0-0-0=regexp&value0-0-0=." .
                        $prodcomp;

    # trick_taint is ok here, as these variables aren't used as a command
    # or in SQL unquoted
    trick_taint($open_name);
    trick_taint($nonopen_name);
    trick_taint($open_query);
    trick_taint($nonopen_query);

    push(@series, [$open_name, $open_query]);
    push(@series, [$nonopen_name, $nonopen_query]);

    foreach my $sdata (@series) {
        my $series = new Bugzilla::Series(undef, $product->name,
                                          $comp_name, $sdata->[0],
                                          $whoid, 1, $sdata->[1], 1);
        $series->writeToDatabase();
    }

    $component =
        new Bugzilla::Component({product_id => $product->id,
                                 name => $comp_name});

    $vars->{'comp'} = $component;
    $vars->{'product'} = $product;
    $template->process("admin/components/created.html.tmpl",
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
    
    $vars->{'comp'} =
        Bugzilla::Component::check_component($product, $comp_name);

    $vars->{'product'} = $product;

    $template->process("admin/components/confirm-delete.html.tmpl", $vars)
        || ThrowTemplateError($template->error());

    exit;
}



#
# action='delete' -> really delete the component
#

if ($action eq 'delete') {

    my $component =
        Bugzilla::Component::check_component($product, $comp_name);

    if ($component->bug_count) {
        if (Param("allowbugdeletion")) {
            foreach my $bug_id (@{$component->bug_ids}) {
                my $bug = new Bugzilla::Bug($bug_id, $whoid);
                $bug->remove_from_db();
            }
        } else {
            ThrowUserError("component_has_bugs",
                           {nb => $component->bug_count });
        }
    }
    
    $dbh->bz_lock_tables('components WRITE', 'flaginclusions WRITE',
                         'flagexclusions WRITE');

    $dbh->do("DELETE FROM flaginclusions WHERE component_id = ?",
             undef, $component->id);
    $dbh->do("DELETE FROM flagexclusions WHERE component_id = ?",
             undef, $component->id);
    $dbh->do("DELETE FROM components WHERE id = ?",
             undef, $component->id);

    $dbh->bz_unlock_tables();

    $vars->{'comp'} = $component;
    $vars->{'product'} = $product;
    $template->process("admin/components/deleted.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}



#
# action='edit' -> present the edit component form
#
# (next action would be 'update')
#

if ($action eq 'edit') {

    $vars->{'comp'} =
        Bugzilla::Component::check_component($product, $comp_name);

    $vars->{'product'} = $product;

    $template->process("admin/components/edit.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}



#
# action='update' -> update the component
#

if ($action eq 'update') {

    # Do the user matching
    Bugzilla::User::match_field ($cgi, {
        'initialowner'     => { 'type' => 'single' },
        'initialqacontact' => { 'type' => 'single' },
    });

    my $comp_old_name         = trim($cgi->param('componentold')     || '');
    my $default_assignee      = trim($cgi->param('initialowner')     || '');
    my $default_qa_contact    = trim($cgi->param('initialqacontact') || '');
    my $description           = trim($cgi->param('description')      || '');

    my $component_old =
        Bugzilla::Component::check_component($product, $comp_old_name);

    $comp_name || ThrowUserError('component_blank_name');

    if (length($comp_name) > 64) {
        ThrowUserError('component_name_too_long',
                       {'name' => $comp_name});
    }

    if ($comp_name ne $component_old->name) {
        my $component =
            new Bugzilla::Component({product_id => $product->id,
                                     name => $comp_name});
        if ($component) {
            ThrowUserError('component_already_exists',
                           {'name' => $component->name});
        }
    }

    $description || ThrowUserError('component_blank_description',
                                   {'name' => $component_old->name});

    $default_assignee || ThrowUserError('component_need_initialowner',
                                        {name => $comp_name});

    my $default_assignee_id   = login_to_id($default_assignee);
    my $default_qa_contact_id = login_to_id($default_qa_contact) || undef;

    $dbh->bz_lock_tables('components WRITE', 'profiles READ');

    if ($comp_name ne $component_old->name) {

        trick_taint($comp_name);
        $dbh->do("UPDATE components SET name = ? WHERE id = ?",
                 undef, ($comp_name, $component_old->id));

        $vars->{'updated_name'} = 1;

    }

    if ($description ne $component_old->description) {
    
        trick_taint($description);
        $dbh->do("UPDATE components SET description = ? WHERE id = ?",
                 undef, ($description, $component_old->id));

        $vars->{'updated_description'} = 1;
    }

    if ($default_assignee ne $component_old->default_assignee->login) {

        $dbh->do("UPDATE components SET initialowner = ? WHERE id = ?",
                 undef, ($default_assignee_id, $component_old->id));

        $vars->{'updated_initialowner'} = 1;
    }

    if (Param('useqacontact')
        && $default_qa_contact ne $component_old->default_qa_contact->login) {
        $dbh->do("UPDATE components SET initialqacontact = ?
                  WHERE id = ?", undef,
                 ($default_qa_contact_id, $component_old->id));

        $vars->{'updated_initialqacontact'} = 1;
    }

    $dbh->bz_unlock_tables();

    my $component = new Bugzilla::Component($component_old->id);
    
    $vars->{'comp'} = $component;
    $vars->{'product'} = $product;
    $template->process("admin/components/updated.html.tmpl",
                       $vars)
      || ThrowTemplateError($template->error());

    exit;
}

#
# No valid action found
#
ThrowUserError('no_valid_action', {'field' => "component"});
