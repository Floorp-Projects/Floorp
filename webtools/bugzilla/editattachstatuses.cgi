#!/usr/bonsaitools/bin/perl -w
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
#                 Myk Melez <myk@mozilla.org>

################################################################################
# Script Initialization
################################################################################

# Make it harder for us to do dangerous things in Perl.
use diagnostics;
use strict;

# Include the Bugzilla CGI and general utility library.
require "CGI.pl";

# Establish a connection to the database backend.
ConnectToDatabase();

# Use the template toolkit (http://www.template-toolkit.org/) to generate
# the user interface (HTML pages and mail messages) using templates in the
# "template/" subdirectory.
use Template;

# Create the global template object that processes templates and specify
# configuration parameters that apply to all templates processed in this script.
my $template = Template->new(
  {
    # Colon-separated list of directories containing templates.
    INCLUDE_PATH => "template/default" ,
    # Allow templates to be specified with relative paths.
    RELATIVE => 1 
  }
);

# Define the global variables and functions that will be passed to the UI 
# template.  Individual functions add their own values to this hash before
# sending them to the templates they process.
my $vars = 
  {
    # Function for retrieving global parameters.
    'Param' => \&Param , 

    # Function for processing global parameters that contain references
    # to other global parameters.
    'PerformSubsts' => \&PerformSubsts
  };

# Make sure the user is logged in and is allowed to edit products
# (i.e. the user has "editcomponents" privileges), since attachment
# statuses are product-specific.
confirm_login();
UserInGroup("editcomponents")
  || DisplayError("You are not authorized to administer attachment statuses.")
  && exit;

################################################################################
# Main Body Execution
################################################################################

# All calls to this script should contain an "action" variable whose value
# determines what the user wants to do.  The code below checks the value of
# that variable and runs the appropriate code.

# Determine whether to use the action specified by the user or the default.
my $action = $::FORM{'action'} || 'list';

if ($action eq "list") 
{ 
  list(); 
} 
elsif ($action eq "create") 
{ 
  create(); 
} 
elsif ($action eq "insert") 
{ 
  validateName();
  validateDescription();
  validateSortKey();
  validateProduct();
  insert();
}
elsif ($action eq "edit") 
{ 
  edit(); 
} 
elsif ($action eq "update") 
{ 
  validateID();
  validateName();
  validateDescription();
  validateSortKey();
  update();
}
elsif ($action eq "delete") 
{ 
  validateID();
  deleteStatus(); 
} 
else 
{ 
  DisplayError("I could not figure out what you wanted to do.")
}

exit;

################################################################################
# Data Validation
################################################################################

sub validateID
{
  $::FORM{'id'} =~ /^[1-9][0-9]*$/
    || DisplayError("The status ID is not a positive integer.") 
      && exit;
  
  SendSQL("SELECT 1 FROM attachstatusdefs WHERE id = $::FORM{'id'}");
  my ($defexists) = FetchSQLData();
  $defexists
    || DisplayError("The status with ID #$::FORM{'id'} does not exist.") 
      && exit;
}

sub validateName
{
  $::FORM{'name'}
    || DisplayError("You must enter a name for the status.")
      && exit;

  $::FORM{'name'} !~ /[\s,]/
    || DisplayError("The status name cannot contain commas or whitespace.")
      && exit;

  length($::FORM{'name'}) <= 50
    || DisplayError("The status name cannot be more than 50 characters long.")
      && exit;
}

sub validateDescription
{
  $::FORM{'desc'}
    || DisplayError("You must enter a description of the status.")
      && exit;
}

sub validateSortKey
{
  $::FORM{'sortkey'} =~ /^\d+$/
    && $::FORM{'sortkey'} < 32768
      || DisplayError("The sort key must be an integer between 0 and 32767 inclusive.") 
        && exit;
}

sub validateProduct
{
  # Retrieve a list of products.
  SendSQL("SELECT product FROM products");
  my @products;
  push(@products, FetchSQLData()) while MoreSQLData();

  grep($_ eq $::FORM{'product'}, @products)
    || DisplayError("You must select an existing product for the status.") 
      && exit;
}

################################################################################
# Functions
################################################################################

sub list
{
  # Administer attachment status flags, which is the set of status flags 
  # that can be applied to an attachment.

  # If the user is seeing this screen as a result of doing something to
  # an attachment status flag, display a message about what happened
  # to that flag (i.e. "The attachment status flag was updated.").
  my ($message) = (@_);

  # Retrieve a list of attachment status flags and create an array of hashes
  # in which each hash contains the data for one flag.
  SendSQL("SELECT id, name, description, sortkey, product 
           FROM attachstatusdefs ORDER BY sortkey");
  my @statusdefs;
  while ( MoreSQLData() )
  {
    my ($id, $name, $description, $sortkey, $product) = FetchSQLData();
    push @statusdefs, { 'id' => $id , 'name' => $name , 'description' => $description , 
                        'sortkey' => $sortkey , 'product' => $product };
  }

  # Define the variables and functions that will be passed to the UI template.
  $vars->{'message'} = $message;
  $vars->{'statusdefs'} = \@statusdefs;

  # Return the appropriate HTTP response headers.
  print "Content-type: text/html\n\n";

  # Generate and return the UI (HTML page) from the appropriate template.
  $template->process("attachstatus/list.atml", $vars)
    || DisplayError("Template process failed: " . $template->error())
    && exit;
}


sub create
{
  # Display a form for creating a new attachment status flag.

  # Retrieve a list of products to which the attachment status may apply.
  SendSQL("SELECT product FROM products");
  my @products;
  push(@products, FetchSQLData()) while MoreSQLData();

  # Define the variables and functions that will be passed to the UI template.
  $vars->{'products'} = \@products;

  # Return the appropriate HTTP response headers.
  print "Content-type: text/html\n\n";

  # Generate and return the UI (HTML page) from the appropriate template.
  $template->process("attachstatus/create.atml", $vars)
    || DisplayError("Template process failed: " . $template->error())
    && exit;
}


sub insert
{
  # Insert a new attachment status flag into the database.

  # Quote the flag's name and description as appropriate for inclusion
  # in a SQL statement.
  my $name = SqlQuote($::FORM{'name'});
  my $desc = SqlQuote($::FORM{'desc'});
  my $product = SqlQuote($::FORM{'product'});

  SendSQL("LOCK TABLES attachstatusdefs WRITE");
  SendSQL("SELECT MAX(id) FROM attachstatusdefs");
  my $id = FetchSQLData() + 1;
  SendSQL("INSERT INTO attachstatusdefs (id, name, description, sortkey, product)
           VALUES ($id, $name, $desc, $::FORM{'sortkey'}, $product)");
  SendSQL("UNLOCK TABLES");

  # Display the "administer attachment status flags" page
  # along with a message that the flag has been created.
  list("The attachment status has been created.");
}


sub edit
{
  # Display a form for editing an existing attachment status flag.

  # Retrieve the definition from the database.
  SendSQL("SELECT name, description, sortkey, product 
           FROM attachstatusdefs WHERE id = $::FORM{'id'}");
  my ($name, $desc, $sortkey, $product) = FetchSQLData();

  # Define the variables and functions that will be passed to the UI template.
  $vars->{'id'} = $::FORM{'id'}; 
  $vars->{'name'} = $name; 
  $vars->{'desc'} = $desc; 
  $vars->{'sortkey'} = $sortkey; 
  $vars->{'product'} = $product; 

  # Return the appropriate HTTP response headers.
  print "Content-type: text/html\n\n";

  # Generate and return the UI (HTML page) from the appropriate template.
  $template->process("attachstatus/edit.atml", $vars)
    || DisplayError("Template process failed: " . $template->error())
    && exit;
}


sub update
{
  # Update an attachment status flag in the database.

  # Quote the flag's name and description as appropriate for inclusion
  # in a SQL statement.
  my $name = SqlQuote($::FORM{'name'});
  my $desc = SqlQuote($::FORM{'desc'});

  SendSQL("LOCK TABLES attachstatusdefs WRITE");
  SendSQL("
           UPDATE  attachstatusdefs 
           SET     name = $name , 
                   description = $desc , 
                   sortkey = $::FORM{'sortkey'} 
           WHERE   id = $::FORM{'id'}
         ");
  SendSQL("UNLOCK TABLES");

  # Display the "administer attachment status flags" page
  # along with a message that the flag has been updated.
  list("The attachment status has been updated.");
}


sub deleteStatus
{
  # Delete an attachment status flag from the database.

  SendSQL("LOCK TABLES attachstatusdefs WRITE, attachstatuses WRITE");
  SendSQL("DELETE FROM attachstatuses WHERE statusid = $::FORM{'id'}");
  SendSQL("DELETE FROM attachstatusdefs WHERE id = $::FORM{'id'}");
  SendSQL("UNLOCK TABLES");

  # Display the "administer attachment status flags" page
  # along with a message that the flag has been deleted.
  list("The attachment status has been deleted.");
}
