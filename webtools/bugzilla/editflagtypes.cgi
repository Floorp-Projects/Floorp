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

################################################################################
# Script Initialization
################################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;
use lib ".";

# Include the Bugzilla CGI and general utility library.
require "CGI.pl";

# Use Bugzilla's flag modules for handling flag types.
use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Flag;
use Bugzilla::FlagType;

use vars qw( $template $vars );

# Make sure the user is logged in and is an administrator.
Bugzilla->login(LOGIN_REQUIRED);
UserInGroup("editcomponents")
  || ThrowUserError("authorization_failure", 
                    { action => "administer flag types" });

# Suppress "used only once" warnings.
use vars qw(@legal_product @legal_components %components);

my $product_id;
my $component_id;

################################################################################
# Main Body Execution
################################################################################

# All calls to this script should contain an "action" variable whose value
# determines what the user wants to do.  The code below checks the value of
# that variable and runs the appropriate code.

# Determine whether to use the action specified by the user or the default.
my $action = $::FORM{'action'} || 'list';

if ($::FORM{'categoryAction'}) {
    processCategoryChange();
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
    # Define the variables and functions that will be passed to the UI template.
    $vars->{'bug_types'} = Bugzilla::FlagType::match({ 'target_type' => 'bug' }, 1);
    $vars->{'attachment_types'} = 
      Bugzilla::FlagType::match({ 'target_type' => 'attachment' }, 1);

    # Return the appropriate HTTP response headers.
    print Bugzilla->cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("admin/flag-type/list.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}


sub edit {
    $action eq 'enter' ? validateTargetType() : validateID();
    
    # Get this installation's products and components.
    GetVersionTable();

    # products and components and the function used to modify the components
    # menu when the products menu changes; used by the template to populate
    # the menus and keep the components menu consistent with the products menu
    $vars->{'products'} = \@::legal_product;
    $vars->{'components'} = \@::legal_components;
    $vars->{'components_by_product'} = \%::components;
    
    $vars->{'last_action'} = $::FORM{'action'};
    if ($::FORM{'action'} eq 'enter' || $::FORM{'action'} eq 'copy') {
        $vars->{'action'} = "insert";
    }
    else { 
        $vars->{'action'} = "update";
    }
    
    # If copying or editing an existing flag type, retrieve it.
    if ($::FORM{'action'} eq 'copy' || $::FORM{'action'} eq 'edit') { 
        $vars->{'type'} = Bugzilla::FlagType::get($::FORM{'id'});
        $vars->{'type'}->{'inclusions'} = Bugzilla::FlagType::get_inclusions($::FORM{'id'});
        $vars->{'type'}->{'exclusions'} = Bugzilla::FlagType::get_exclusions($::FORM{'id'});
    }
    # Otherwise set the target type (the minimal information about the type
    # that the template needs to know) from the URL parameter and default
    # the list of inclusions to all categories.
    else { 
        $vars->{'type'} = { 'target_type' => $::FORM{'target_type'} , 
                            'inclusions'  => ["__Any__:__Any__"] };
    }
    
    # Return the appropriate HTTP response headers.
    print Bugzilla->cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("admin/flag-type/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

sub processCategoryChange {
    validateIsActive();
    validateIsRequestable();
    validateIsRequesteeble();
    validateAllowMultiple();
    
    my @inclusions = $::MFORM{'inclusions'} ? @{$::MFORM{'inclusions'}} : ();
    my @exclusions = $::MFORM{'exclusions'} ? @{$::MFORM{'exclusions'}} : ();
    if ($::FORM{'categoryAction'} eq "Include") {
        validateProduct();
        validateComponent();
        my $category = ($::FORM{'product'} || "__Any__") . ":" . ($::FORM{'component'} || "__Any__");
        push(@inclusions, $category) unless grep($_ eq $category, @inclusions);
    }
    elsif ($::FORM{'categoryAction'} eq "Exclude") {
        validateProduct();
        validateComponent();
        my $category = ($::FORM{'product'} || "__Any__") . ":" . ($::FORM{'component'} || "__Any__");
        push(@exclusions, $category) unless grep($_ eq $category, @exclusions);
    }
    elsif ($::FORM{'categoryAction'} eq "Remove Inclusion") {
        @inclusions = map(($_ eq $::FORM{'inclusion_to_remove'} ? () : $_), @inclusions);
    }
    elsif ($::FORM{'categoryAction'} eq "Remove Exclusion") {
        @exclusions = map(($_ eq $::FORM{'exclusion_to_remove'} ? () : $_), @exclusions);
    }
    
    # Get this installation's products and components.
    GetVersionTable();

    # products and components; used by the template to populate the menus 
    # and keep the components menu consistent with the products menu
    $vars->{'products'} = \@::legal_product;
    $vars->{'components'} = \@::legal_components;
    $vars->{'components_by_product'} = \%::components;
    
    $vars->{'action'} = $::FORM{'action'};
    my $type = {};
    foreach my $key (keys %::FORM) { $type->{$key} = $::FORM{$key} }
    $type->{'inclusions'} = \@inclusions;
    $type->{'exclusions'} = \@exclusions;
    $vars->{'type'} = $type;
    
    # Return the appropriate HTTP response headers.
    print Bugzilla->cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("admin/flag-type/edit.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

sub insert {
    validateName();
    validateDescription();
    validateCCList();
    validateTargetType();
    validateSortKey();
    validateIsActive();
    validateIsRequestable();
    validateIsRequesteeble();
    validateAllowMultiple();
    
    my $name = SqlQuote($::FORM{'name'});
    my $description = SqlQuote($::FORM{'description'});
    my $cc_list = SqlQuote($::FORM{'cc_list'});
    my $target_type = $::FORM{'target_type'} eq "bug" ? "b" : "a";
    
    SendSQL("LOCK TABLES flagtypes WRITE, products READ, components READ, " . 
            "flaginclusions WRITE, flagexclusions WRITE");
    
    # Determine the new flag type's unique identifier.
    SendSQL("SELECT MAX(id) FROM flagtypes");
    my $id = FetchSQLData() + 1;
    
    # Insert a record for the new flag type into the database.
    SendSQL("INSERT INTO flagtypes (id, name, description, cc_list, 
                 target_type, sortkey, is_active, is_requestable, 
                 is_requesteeble, is_multiplicable) 
             VALUES ($id, $name, $description, $cc_list, '$target_type', 
                 $::FORM{'sortkey'}, $::FORM{'is_active'}, 
                 $::FORM{'is_requestable'}, $::FORM{'is_requesteeble'}, 
                 $::FORM{'is_multiplicable'})");
    
    # Populate the list of inclusions/exclusions for this flag type.
    foreach my $category_type ("inclusions", "exclusions") {
        foreach my $category (@{$::MFORM{$category_type}}) {
          my ($product, $component) = split(/:/, $category);
          my $product_id = get_product_id($product) || "NULL";
          my $component_id = 
            get_component_id($product_id, $component) || "NULL";
          SendSQL("INSERT INTO flag$category_type (type_id, product_id, " . 
                  "component_id) VALUES ($id, $product_id, $component_id)");
        }
    }
    
    SendSQL("UNLOCK TABLES");

    $vars->{'name'} = $::FORM{'name'};
    $vars->{'message'} = "flag_type_created";

    # Return the appropriate HTTP response headers.
    print Bugzilla->cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}


sub update {
    validateID();
    validateName();
    validateDescription();
    validateCCList();
    validateTargetType();
    validateSortKey();
    validateIsActive();
    validateIsRequestable();
    validateIsRequesteeble();
    validateAllowMultiple();
    
    my $name = SqlQuote($::FORM{'name'});
    my $description = SqlQuote($::FORM{'description'});
    my $cc_list = SqlQuote($::FORM{'cc_list'});
    
    SendSQL("LOCK TABLES flagtypes WRITE, products READ, components READ, " . 
            "flaginclusions WRITE, flagexclusions WRITE");
    SendSQL("UPDATE  flagtypes 
                SET  name = $name , 
                     description = $description , 
                     cc_list = $cc_list , 
                     sortkey = $::FORM{'sortkey'} , 
                     is_active = $::FORM{'is_active'} , 
                     is_requestable = $::FORM{'is_requestable'} , 
                     is_requesteeble = $::FORM{'is_requesteeble'} , 
                     is_multiplicable = $::FORM{'is_multiplicable'} 
              WHERE  id = $::FORM{'id'}");
    
    # Update the list of inclusions/exclusions for this flag type.
    foreach my $category_type ("inclusions", "exclusions") {
        SendSQL("DELETE FROM flag$category_type WHERE type_id = $::FORM{'id'}");
        foreach my $category (@{$::MFORM{$category_type}}) {
          my ($product, $component) = split(/:/, $category);
          my $product_id = get_product_id($product) || "NULL";
          my $component_id = 
            get_component_id($product_id, $component) || "NULL";
          SendSQL("INSERT INTO flag$category_type (type_id, product_id, " . 
                  "component_id) VALUES ($::FORM{'id'}, $product_id, " . 
                  "$component_id)");
        }
    }

    SendSQL("UNLOCK TABLES");
    
    # Clear existing flags for bugs/attachments in categories no longer on 
    # the list of inclusions or that have been added to the list of exclusions.
    SendSQL("
        SELECT flags.id 
        FROM flags, bugs LEFT OUTER JOIN flaginclusions AS i
        ON (flags.type_id = i.type_id 
            AND (bugs.product_id = i.product_id OR i.product_id IS NULL)
            AND (bugs.component_id = i.component_id OR i.component_id IS NULL))
        WHERE flags.type_id = $::FORM{'id'} 
        AND flags.bug_id = bugs.bug_id
        AND flags.is_active = 1
        AND i.type_id IS NULL
    ");
    Bugzilla::Flag::clear(FetchOneColumn()) while MoreSQLData();
    
    SendSQL("
        SELECT flags.id 
        FROM flags, bugs, flagexclusions AS e
        WHERE flags.type_id = $::FORM{'id'}
        AND flags.bug_id = bugs.bug_id
        AND flags.type_id = e.type_id 
        AND flags.is_active = 1
        AND (bugs.product_id = e.product_id OR e.product_id IS NULL)
        AND (bugs.component_id = e.component_id OR e.component_id IS NULL)
    ");
    Bugzilla::Flag::clear(FetchOneColumn()) while MoreSQLData();
    
    $vars->{'name'} = $::FORM{'name'};
    $vars->{'message'} = "flag_type_changes_saved";

    # Return the appropriate HTTP response headers.
    print Bugzilla->cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}


sub confirmDelete 
{
  validateID();
  # check if we need confirmation to delete:
  
  my $count = Bugzilla::Flag::count({ 'type_id' => $::FORM{'id'},
                                      'is_active' => 1 });
  
  if ($count > 0) {
    $vars->{'flag_type'} = Bugzilla::FlagType::get($::FORM{'id'});
    $vars->{'flag_count'} = scalar($count);

    # Return the appropriate HTTP response headers.
    print Bugzilla->cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("admin/flag-type/confirm-delete.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
  } 
  else {
    deleteType();
  }
}


sub deleteType {
    validateID();
    
    SendSQL("LOCK TABLES flagtypes WRITE, flags WRITE, " . 
            "flaginclusions WRITE, flagexclusions WRITE");
    
    # Get the name of the flag type so we can tell users
    # what was deleted.
    SendSQL("SELECT name FROM flagtypes WHERE id = $::FORM{'id'}");
    $vars->{'name'} = FetchOneColumn();
    
    SendSQL("DELETE FROM flags WHERE type_id = $::FORM{'id'}");
    SendSQL("DELETE FROM flaginclusions WHERE type_id = $::FORM{'id'}");
    SendSQL("DELETE FROM flagexclusions WHERE type_id = $::FORM{'id'}");
    SendSQL("DELETE FROM flagtypes WHERE id = $::FORM{'id'}");
    SendSQL("UNLOCK TABLES");

    $vars->{'message'} = "flag_type_deleted";

    # Return the appropriate HTTP response headers.
    print Bugzilla->cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}


sub deactivate {
    validateID();
    validateIsActive();
    
    SendSQL("LOCK TABLES flagtypes WRITE");
    SendSQL("UPDATE flagtypes SET is_active = 0 WHERE id = $::FORM{'id'}");
    SendSQL("UNLOCK TABLES");
    
    $vars->{'message'} = "flag_type_deactivated";
    $vars->{'flag_type'} = Bugzilla::FlagType::get($::FORM{'id'});
    
    # Return the appropriate HTTP response headers.
    print Bugzilla->cgi->header();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}


################################################################################
# Data Validation / Security Authorization
################################################################################

sub validateID {
    detaint_natural($::FORM{'id'})
      || ThrowCodeError("flag_type_id_invalid", { id => $::FORM{'id'} });

    SendSQL("SELECT 1 FROM flagtypes WHERE id = $::FORM{'id'}");
    FetchOneColumn()
      || ThrowCodeError("flag_type_nonexistent", { id => $::FORM{'id'} });
}

sub validateName {
    $::FORM{'name'}
      && length($::FORM{'name'}) <= 50
      || ThrowUserError("flag_type_name_invalid", { name => $::FORM{'name'} });
}

sub validateDescription {
    length($::FORM{'description'}) < 2**16-1
      || ThrowUserError("flag_type_description_invalid");
}

sub validateCCList {
    length($::FORM{'cc_list'}) <= 200
      || ThrowUserError("flag_type_cc_list_invalid", 
                        { cc_list => $::FORM{'cc_list'} });
    
    my @addresses = split(/[, ]+/, $::FORM{'cc_list'});
    foreach my $address (@addresses) { CheckEmailSyntax($address) }
}

sub validateProduct {
    return if !$::FORM{'product'};
    
    $product_id = get_product_id($::FORM{'product'});
    
    defined($product_id)
      || ThrowCodeError("flag_type_product_nonexistent", 
                        { product => $::FORM{'product'} });
}

sub validateComponent {
    return if !$::FORM{'component'};
    
    $product_id
      || ThrowCodeError("flag_type_component_without_product");
    
    $component_id = get_component_id($product_id, $::FORM{'component'});

    defined($component_id)
      || ThrowCodeError("flag_type_component_nonexistent", 
                        { product   => $::FORM{'product'},
                          component => $::FORM{'component'} });
}

sub validateSortKey {
    detaint_natural($::FORM{'sortkey'})
      && $::FORM{'sortkey'} < 32768
      || ThrowUserError("flag_type_sortkey_invalid", 
                        { sortkey => $::FORM{'sortkey'} });
}

sub validateTargetType {
    grep($::FORM{'target_type'} eq $_, ("bug", "attachment"))
      || ThrowCodeError("flag_type_target_type_invalid", 
                        { target_type => $::FORM{'target_type'} });
}

sub validateIsActive {
    $::FORM{'is_active'} = $::FORM{'is_active'} ? 1 : 0;
}

sub validateIsRequestable {
    $::FORM{'is_requestable'} = $::FORM{'is_requestable'} ? 1 : 0;
}

sub validateIsRequesteeble {
    $::FORM{'is_requesteeble'} = $::FORM{'is_requesteeble'} ? 1 : 0;
}

sub validateAllowMultiple {
    $::FORM{'is_multiplicable'} = $::FORM{'is_multiplicable'} ? 1 : 0;
}

