#!/usr/bonsaitools/bin/perl -wT
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
# Contributor(s): Jacob Steenhagen <jake@acutex.net>
#

# Suppress silly "used only once" warnings
use vars qw{ %COOKIE };


###############################################################################
# Script Initialization
###############################################################################

# Make it harder for us to do dangerous things in Perl.
use diagnostics;
use strict;

# Include the Bugzilla CGI and general utility library.
use lib ".";
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
    INCLUDE_PATH => "template/custom:template/default",
    # Allow templates to be specified with relative paths.
    RELATIVE => 1,
    POST_CHOMP => 1,
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

# Check whether or not the user is logged in and, if so, set the $::userid 
# and $::usergroupset variables.
quietly_check_login();

###############################################################################
# Main Body Execution
###############################################################################

$vars->{'username'} = $::COOKIE{'Bugzilla_login'} || '';
$vars->{'subst'} = { 'userid' => $vars->{'username'} };

# Return the appropriate HTTP response headers.
print "Content-Type: text/html\n\n";

# Generate and return the UI (HTML page) from the appropriate template.
$template->process("index.tmpl", $vars)
  || DisplayError("Template process failed: " . $template->error())
  && exit;
