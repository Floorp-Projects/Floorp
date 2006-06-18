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
# Contributor(s): Myk Melez <myk@mozilla.org>
#                 Frédéric Buclin <LpSolit@gmail.com>

################################################################################
# Script Initialization
################################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;
use lib ".";

# Include the Bugzilla CGI and general utility library.
require "globals.pl";

# Use Bugzilla's flag modules for handling flag types.
use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Flag;
use Bugzilla::FlagType;
use Bugzilla::Group;
use Bugzilla::Util;
use Bugzilla::Product;
use Bugzilla::Component;
use Bugzilla::Bug;
use Bugzilla::Attachment;

use List::Util qw(reduce);

my $template = Bugzilla->template;
my $vars = {};

# Make sure the user is logged in and is an administrator.
my $user = Bugzilla->login(LOGIN_REQUIRED);
$user->in_group('editcomponents')
  || ThrowUserError("auth_failure", {group  => "editcomponents",
                                     action => "edit",
                                     object => "flagtypes"});

my $cgi = Bugzilla->cgi;

################################################################################
# Main Body Execution
################################################################################

# All calls to this script should contain an "action" variable whose value
# determines what the user wants to do.  The code below checks the value of
# that variable and runs the appropriate code.

# Determine whether to use the action specified by the user or the default.
my $action = $cgi->param('action') || 'list';
my @categoryActions;

if (@categoryActions = grep(/^categoryAction-.+/, $cgi->param())) {
    $categoryActions[0] =~ s/^categoryAction-//;
    processCategoryChange($categoryActions[0]);
    exit;
}

if    ($action eq 'list')           { list();           }
elsif ($action eq 'enter')          { edit();           }
elsif ($action eq 'copy')           { edit();           }
elsif ($action eq 'edit')           { edit();           }
elsif ($action eq 'insert')         { insert();         }
elsif ($action eq 'update')         { update();         }
elsif ($action eq 'confirmdelete')  { confirmDelete();  } 
elsif ($action eq 'delete')         { deleteType();     }
elsif ($action eq 'deactivate')     { deactivate();     }
else { 
    ThrowCodeError("action_unrecognized", { action => $action });
}

exit;

################################################################################
# Functions
################################################################################

sub list {
    # Restrict the list to the given product and component, if given.
    $vars = get_products_and_components($vars);

    my $product = validateProduct(scalar $cgi->param('product'));
    my $component = validateComponent($product, scalar $cgi->param('component'));
    my $product_id = $product ? $product->id : 0;
    my $component_id = $component ? $component->id : 0;

    # Define the variables and functions that will be passed to the UI template.
    $vars->{'selected_product'} = $cgi->param('product');
    $vars->{'selected_component'} = $cgi->param('component');

    # If only a product was specified but no component, then we restrict
    # the list to flag types available in ALL components of that product.
    my @comp_ids = ($component_id);
    if ($product_id && !$component_id) {
        @comp_ids = map {$_->id} @{$product->components};
    }

    my @bug_flagtypes;
    my @attach_flagtypes;

    foreach my $comp_id (@comp_ids) {
        my $bug_types =
          Bugzilla::FlagType::match({ 'target_type' => 'bug',
                                      'group' => scalar $cgi->param('group'),
                                      'product_id' => $product_id,
                                      'component_id' => $comp_id }, 1);
        push(@bug_flagtypes, $bug_types);

        my $attach_types =
          Bugzilla::FlagType::match({ 'target_type' => 'attachment',
                                      'group' => scalar $cgi->param('group'),
                                      'product_id' => $product_id,
                                      'component_id' => $comp_id }, 1);
        push(@attach_flagtypes, $attach_types);
    }

    sub intersection {
        my ($aa, $bb) = @_;
        my %union;
        my %isect;
        foreach my $e (@$aa, @$bb) { $union{$e->{'id'}}++ && ($isect{$e->{'id'}} ||= $e) };
        return [sort { $a->{'sortkey'} <=> $b->{'sortkey'}
                       || $a->{'name'} cmp $b->{'name'} } values %isect];
    }
    $vars->{'bug_types'} = reduce { intersection($a, $b) } @bug_flagtypes;
    $vars->{'attachment_types'} = reduce { intersection($a, $b) } @attach_flagtypes;

    # Users want to see group names, not IDs
    # So get the group names
    my %group_name_cache = ();
    foreach my $flag_type_set ("bug_types", "attachment_types") {
        foreach my $flag_type (@{$vars->{$flag_type_set}}) {
            foreach my $group ("grant", "request") {
                my $gid = $flag_type->{$group . "_gid"};
                next if (!$gid);
                $group_name_cache{$gid} ||= new Bugzilla::Group($gid)->name();
                $flag_type->{$group . "_group_name"} = $group_name_cache{$gid};
            }
        }
    }

    # Return the appropriate HTTP response headers.
    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("admin/flag-type/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}


sub edit {
    $action eq 'enter' ? validateTargetType() : (my $id = validateID());
    my $dbh = Bugzilla->dbh;

    # Fill $vars with products and components data.
    $vars = get_products_and_components($vars);

    $vars->{'last_action'} = $cgi->param('action');
    if ($cgi->param('action') eq 'enter' || $cgi->param('action') eq 'copy') {
        $vars->{'action'} = "insert";
    }
    else { 
        $vars->{'action'} = "update";
    }
    
    # If copying or editing an existing flag type, retrieve it.
    if ($cgi->param('action') eq 'copy' || $cgi->param('action') eq 'edit') { 
        $vars->{'type'} = Bugzilla::FlagType::get($id);
        $vars->{'type'}->{'inclusions'} = Bugzilla::FlagType::get_inclusions($id);
        $vars->{'type'}->{'exclusions'} = Bugzilla::FlagType::get_exclusions($id);
        # Users want to see group names, not IDs
        foreach my $group ("grant_gid", "request_gid") {
            my $gid = $vars->{'type'}->{$group};
            next if (!$gid);
            ($vars->{'type'}->{$group}) =
                $dbh->selectrow_array('SELECT name FROM groups WHERE id = ?',
                                       undef, $gid);
        }
    }
    # Otherwise set the target type (the minimal information about the type
    # that the template needs to know) from the URL parameter and default
    # the list of inclusions to all categories.
    else {
        my %inclusions;
        $inclusions{"__Any__:__Any__"} = "0:0";
        $vars->{'type'} = { 'target_type' => scalar $cgi->param('target_type'),
                            'inclusions'  => \%inclusions };
    }
    # Get a list of groups available to restrict this flag type against.
    my @groups = Bugzilla::Group::get_all_groups();
    $vars->{'groups'} = \@groups;
    # Return the appropriate HTTP response headers.
    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("admin/flag-type/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

sub processCategoryChange {
    my $categoryAction = shift;
    validateIsActive();
    validateIsRequestable();
    validateIsRequesteeble();
    validateAllowMultiple();
    
    my @inclusions = $cgi->param('inclusions');
    my @exclusions = $cgi->param('exclusions');
    if ($categoryAction eq 'include') {
        my $product = validateProduct(scalar $cgi->param('product'));
        my $component = validateComponent($product, scalar $cgi->param('component'));
        my $category = ($product ? $product->id : 0) . ":" .
                       ($component ? $component->id : 0);
        push(@inclusions, $category) unless grep($_ eq $category, @inclusions);
    }
    elsif ($categoryAction eq 'exclude') {
        my $product = validateProduct(scalar $cgi->param('product'));
        my $component = validateComponent($product, scalar $cgi->param('component'));
        my $category = ($product ? $product->id : 0) . ":" .
                       ($component ? $component->id : 0);
        push(@exclusions, $category) unless grep($_ eq $category, @exclusions);
    }
    elsif ($categoryAction eq 'removeInclusion') {
        my @inclusion_to_remove = $cgi->param('inclusion_to_remove');
        @inclusions = map {(lsearch(\@inclusion_to_remove, $_) < 0) ? $_ : ()} @inclusions;
    }
    elsif ($categoryAction eq 'removeExclusion') {
        my @exclusion_to_remove = $cgi->param('exclusion_to_remove');
        @exclusions = map {(lsearch(\@exclusion_to_remove, $_) < 0) ? $_ : ()} @exclusions;
    }
    
    # Convert the array @clusions('prod_ID:comp_ID') back to a hash of
    # the form %clusions{'prod_name:comp_name'} = 'prod_ID:comp_ID'
    my %inclusions = clusion_array_to_hash(\@inclusions);
    my %exclusions = clusion_array_to_hash(\@exclusions);

    # Fill $vars with products and components data.
    $vars = get_products_and_components($vars);

    my @groups = Bugzilla::Group::get_all_groups();
    $vars->{'groups'} = \@groups;
    $vars->{'action'} = $cgi->param('action');

    my $type = {};
    foreach my $key ($cgi->param()) { $type->{$key} = $cgi->param($key) }
    $type->{'inclusions'} = \%inclusions;
    $type->{'exclusions'} = \%exclusions;
    $vars->{'type'} = $type;
    
    # Return the appropriate HTTP response headers.
    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("admin/flag-type/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

# Convert the array @clusions('prod_ID:comp_ID') back to a hash of
# the form %clusions{'prod_name:comp_name'} = 'prod_ID:comp_ID'
sub clusion_array_to_hash {
    my $array = shift;
    my %hash;
    my %products;
    my %components;
    foreach my $ids (@$array) {
        trick_taint($ids);
        my ($product_id, $component_id) = split(":", $ids);
        my $product_name = "__Any__";
        if ($product_id) {
            $products{$product_id} ||= new Bugzilla::Product($product_id);
            $product_name = $products{$product_id}->name if $products{$product_id};
        }
        my $component_name = "__Any__";
        if ($component_id) {
            $components{$component_id} ||= new Bugzilla::Component($component_id);
            $component_name = $components{$component_id}->name if $components{$component_id};
        }
        $hash{"$product_name:$component_name"} = $ids;
    }
    return %hash;
}

sub insert {
    my $name = validateName();
    my $description = validateDescription();
    my $cc_list = validateCCList();
    validateTargetType();
    validateSortKey();
    validateIsActive();
    validateIsRequestable();
    validateIsRequesteeble();
    validateAllowMultiple();
    validateGroups();

    my $dbh = Bugzilla->dbh;

    my $target_type = $cgi->param('target_type') eq "bug" ? "b" : "a";

    $dbh->bz_lock_tables('flagtypes WRITE', 'products READ',
                         'components READ', 'flaginclusions WRITE',
                         'flagexclusions WRITE');

    # Determine the new flag type's unique identifier.
    my $id = $dbh->selectrow_array('SELECT MAX(id) FROM flagtypes') + 1;

    # Insert a record for the new flag type into the database.
    $dbh->do('INSERT INTO flagtypes
                          (id, name, description, cc_list, target_type,
                           sortkey, is_active, is_requestable, 
                           is_requesteeble, is_multiplicable, 
                           grant_group_id, request_group_id) 
                   VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)',
              undef, ($id, $name, $description, $cc_list, $target_type,
                      $cgi->param('sortkey'), $cgi->param('is_active'),
                      $cgi->param('is_requestable'), $cgi->param('is_requesteeble'),
                      $cgi->param('is_multiplicable'), scalar($cgi->param('grant_gid')),
                      scalar($cgi->param('request_gid'))));

    # Populate the list of inclusions/exclusions for this flag type.
    validateAndSubmit($id);

    $dbh->bz_unlock_tables();

    $vars->{'name'} = $cgi->param('name');
    $vars->{'message'} = "flag_type_created";

    # Return the appropriate HTTP response headers.
    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}


sub update {
    my $id = validateID();
    my $name = validateName();
    my $description = validateDescription();
    my $cc_list = validateCCList();
    validateTargetType();
    validateSortKey();
    validateIsActive();
    validateIsRequestable();
    validateIsRequesteeble();
    validateAllowMultiple();
    validateGroups();

    my $dbh = Bugzilla->dbh;
    $dbh->bz_lock_tables('flagtypes WRITE', 'products READ',
                         'components READ', 'flaginclusions WRITE',
                         'flagexclusions WRITE');
    $dbh->do('UPDATE flagtypes
                 SET name = ?, description = ?, cc_list = ?,
                     sortkey = ?, is_active = ?, is_requestable = ?,
                     is_requesteeble = ?, is_multiplicable = ?,
                     grant_group_id = ?, request_group_id = ?
               WHERE id = ?',
              undef, ($name, $description, $cc_list, $cgi->param('sortkey'),
                      $cgi->param('is_active'), $cgi->param('is_requestable'),
                      $cgi->param('is_requesteeble'), $cgi->param('is_multiplicable'),
                      scalar($cgi->param('grant_gid')), scalar($cgi->param('request_gid')),
                      $id));
    
    # Update the list of inclusions/exclusions for this flag type.
    validateAndSubmit($id);

    $dbh->bz_unlock_tables();

    # Clear existing flags for bugs/attachments in categories no longer on 
    # the list of inclusions or that have been added to the list of exclusions.
    my $flags =    $dbh->selectall_arrayref('SELECT flags.id, flags.bug_id, flags.attach_id
                                               FROM flags
                                         INNER JOIN bugs
                                                 ON flags.bug_id = bugs.bug_id
                                    LEFT OUTER JOIN flaginclusions AS i
                                                 ON (flags.type_id = i.type_id 
                                                     AND (bugs.product_id = i.product_id
                                                          OR i.product_id IS NULL)
                                                     AND (bugs.component_id = i.component_id
                                                          OR i.component_id IS NULL))
                                              WHERE flags.type_id = ?
                                                AND i.type_id IS NULL',
                                             undef, $id);
    foreach my $flag (@$flags) {
        my ($flag_id, $bug_id, $attach_id) = @$flag;
        my $bug = new Bugzilla::Bug($bug_id, $user->id);
        my $attachment = $attach_id ? Bugzilla::Attachment->get($attach_id) : undef;
        Bugzilla::Flag::clear($flag_id, $bug, $attachment);
    }

    $flags =    $dbh->selectall_arrayref('SELECT flags.id, flags.bug_id, flags.attach_id
                                            FROM flags
                                      INNER JOIN bugs 
                                              ON flags.bug_id = bugs.bug_id
                                      INNER JOIN flagexclusions AS e
                                              ON flags.type_id = e.type_id
                                           WHERE flags.type_id = ?
                                             AND (bugs.product_id = e.product_id
                                                  OR e.product_id IS NULL)
                                             AND (bugs.component_id = e.component_id
                                                  OR e.component_id IS NULL)',
                                          undef, $id);
    foreach my $flag (@$flags) {
        my ($flag_id, $bug_id, $attach_id) = @$flag;
        my $bug = new Bugzilla::Bug($bug_id, $user->id);
        my $attachment = $attach_id ? Bugzilla::Attachment->get($attach_id) : undef;
        Bugzilla::Flag::clear($flag_id, $bug, $attachment);
    }

    $vars->{'name'} = $cgi->param('name');
    $vars->{'message'} = "flag_type_changes_saved";

    # Return the appropriate HTTP response headers.
    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}


sub confirmDelete 
{
  my $id = validateID();

  # check if we need confirmation to delete:
  
  my $count = Bugzilla::Flag::count({ 'type_id' => $id });
  
  if ($count > 0) {
    $vars->{'flag_type'} = Bugzilla::FlagType::get($id);
    $vars->{'flag_count'} = scalar($count);

    # Return the appropriate HTTP response headers.
    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("admin/flag-type/confirm-delete.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
  } 
  else {
    deleteType();
  }
}


sub deleteType {
    my $id = validateID();
    my $dbh = Bugzilla->dbh;

    $dbh->bz_lock_tables('flagtypes WRITE', 'flags WRITE',
                         'flaginclusions WRITE', 'flagexclusions WRITE');
    
    # Get the name of the flag type so we can tell users
    # what was deleted.
    ($vars->{'name'}) = $dbh->selectrow_array('SELECT name FROM flagtypes
                                               WHERE id = ?', undef, $id);

    $dbh->do('DELETE FROM flags WHERE type_id = ?', undef, $id);
    $dbh->do('DELETE FROM flaginclusions WHERE type_id = ?', undef, $id);
    $dbh->do('DELETE FROM flagexclusions WHERE type_id = ?', undef, $id);
    $dbh->do('DELETE FROM flagtypes WHERE id = ?', undef, $id);
    $dbh->bz_unlock_tables();

    $vars->{'message'} = "flag_type_deleted";

    # Return the appropriate HTTP response headers.
    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}


sub deactivate {
    my $id = validateID();
    validateIsActive();

    my $dbh = Bugzilla->dbh;

    $dbh->bz_lock_tables('flagtypes WRITE');
    $dbh->do('UPDATE flagtypes SET is_active = 0 WHERE id = ?', undef, $id);
    $dbh->bz_unlock_tables();
    
    $vars->{'message'} = "flag_type_deactivated";
    $vars->{'flag_type'} = Bugzilla::FlagType::get($id);
    
    # Return the appropriate HTTP response headers.
    print $cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

sub get_products_and_components {
    my $vars = shift;

    my @products = Bugzilla::Product::get_all_products();
    # We require all unique component names.
    my %components;
    foreach my $product (@products) {
        foreach my $component (@{$product->components}) {
            $components{$component->name} = 1;
        }
    }
    $vars->{'products'} = \@products;
    $vars->{'components'} = [sort(keys %components)];
    return $vars;
}

################################################################################
# Data Validation / Security Authorization
################################################################################

sub validateID {
    my $dbh = Bugzilla->dbh;
    # $flagtype_id is destroyed if detaint_natural fails.
    my $flagtype_id = $cgi->param('id');
    detaint_natural($flagtype_id)
      || ThrowCodeError("flag_type_id_invalid",
                        { id => scalar $cgi->param('id') });

    my $flagtype_exists =
        $dbh->selectrow_array('SELECT 1 FROM flagtypes WHERE id = ?',
                               undef, $flagtype_id);
    $flagtype_exists
      || ThrowCodeError("flag_type_nonexistent", { id => $flagtype_id });

    return $flagtype_id;
}

sub validateName {
    my $name = $cgi->param('name');
    ($name && $name !~ /[ ,]/ && length($name) <= 50)
      || ThrowUserError("flag_type_name_invalid",
                        { name => $name });
    trick_taint($name);
    return $name;
}

sub validateDescription {
    my $description = $cgi->param('description');
    length($description) < 2**16-1
      || ThrowUserError("flag_type_description_invalid");
    trick_taint($description);
    return $description;
}

sub validateCCList {
    my $cc_list = $cgi->param('cc_list');
    length($cc_list) <= 200
      || ThrowUserError("flag_type_cc_list_invalid", 
                        { cc_list => $cc_list });

    my @addresses = split(/[, ]+/, $cc_list);
    # We do not call Util::validate_email_syntax because these
    # addresses do not require to match 'emailregexp' and do not
    # depend on 'emailsuffix'. So we limit ourselves to a simple
    # sanity check:
    # - match the syntax of a fully qualified email address;
    # - do not contain any illegal character.
    foreach my $address (@addresses) {
        ($address =~ /^[\w\.\+\-=]+@[\w\.\-]+\.[\w\-]+$/
           && $address !~ /[\\\(\)<>&,;:"\[\] \t\r\n]/)
          || ThrowUserError('illegal_email_address',
                            {addr => $address, default => 1});
    }
    trick_taint($cc_list);
    return $cc_list;
}

sub validateProduct {
    my $product_name = shift;
    return unless $product_name;

    my $product = Bugzilla::Product::check_product($product_name);
    return $product;
}

sub validateComponent {
    my ($product, $component_name) = @_;
    return unless $component_name;

    ($product && $product->id)
      || ThrowUserError("flag_type_component_without_product");

    my $component = Bugzilla::Component::check_component($product, $component_name);
    return $component;
}

sub validateSortKey {
    # $sortkey is destroyed if detaint_natural fails.
    my $sortkey = $cgi->param('sortkey');
    detaint_natural($sortkey)
      && $sortkey < 32768
      || ThrowUserError("flag_type_sortkey_invalid", 
                        { sortkey => scalar $cgi->param('sortkey') });
    $cgi->param('sortkey', $sortkey);
}

sub validateTargetType {
    grep($cgi->param('target_type') eq $_, ("bug", "attachment"))
      || ThrowCodeError("flag_type_target_type_invalid", 
                        { target_type => scalar $cgi->param('target_type') });
}

sub validateIsActive {
    $cgi->param('is_active', $cgi->param('is_active') ? 1 : 0);
}

sub validateIsRequestable {
    $cgi->param('is_requestable', $cgi->param('is_requestable') ? 1 : 0);
}

sub validateIsRequesteeble {
    $cgi->param('is_requesteeble', $cgi->param('is_requesteeble') ? 1 : 0);
}

sub validateAllowMultiple {
    $cgi->param('is_multiplicable', $cgi->param('is_multiplicable') ? 1 : 0);
}

sub validateGroups {
    my $dbh = Bugzilla->dbh;
    # Convert group names to group IDs
    foreach my $col ("grant_gid", "request_gid") {
      my $name = $cgi->param($col);
      if ($name) {
          trick_taint($name);
          my $gid = $dbh->selectrow_array('SELECT id FROM groups
                                           WHERE name = ?', undef, $name);
          $gid || ThrowUserError("group_unknown", { name => $name });
          $cgi->param($col, $gid);
      }
      else {
          $cgi->delete($col);
      }
    }
}

# At this point, values either come the DB itself or have been recently
# added by the user and have passed all validation tests.
# The only way to have invalid product/component combinations is to
# hack the URL. So we silently ignore them, if any.
sub validateAndSubmit {
    my ($id) = @_;
    my $dbh = Bugzilla->dbh;

    # Cache product objects.
    my %products;
    foreach my $category_type ("inclusions", "exclusions") {
        # Will be used several times below.
        my $sth = $dbh->prepare("INSERT INTO flag$category_type " .
                                "(type_id, product_id, component_id) " .
                                "VALUES (?, ?, ?)");

        $dbh->do("DELETE FROM flag$category_type WHERE type_id = ?", undef, $id);
        foreach my $category ($cgi->param($category_type)) {
            trick_taint($category);
            my ($product_id, $component_id) = split(":", $category);
            # Does the product exist?
            if ($product_id) {
                $products{$product_id} ||= new Bugzilla::Product($product_id);
                next unless defined $products{$product_id};
            }
            # A component was selected without a product being selected.
            next if (!$product_id && $component_id);
            # Does the component belong to this product?
            if ($component_id) {
                my @match = grep {$_->id == $component_id} @{$products{$product_id}->components};
                next unless scalar(@match);
            }
            $product_id ||= undef;
            $component_id ||= undef;
            $sth->execute($id, $product_id, $component_id);
        }
    }
}
