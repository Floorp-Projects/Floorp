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

use strict;

use lib ".";

use Bugzilla;
use Bugzilla::Field;
use Getopt::Long;

my ($name, $desc);
my $result = GetOptions("name=s"             => \$name,
                        "description|desc=s" => \$desc);

if (!$name or !$desc) {
    my $command =
      $^O =~ /MSWin32/i ? "perl -T customfield.pl" : "./customfield.pl";
    print <<END;
Usage:

  Use this script to add a custom field to your Bugzilla installation
  by invoking it with the --name and --desc command-line options:

  $command --name=<field_name> --desc="<field description>"

  <field_name> is the name of the custom field in the database.
  The string "cf_" will be prepended to this name to distinguish
  the field from standard fields.  This name must conform to the
  naming rules for the database server you use.
  
  <field description> is a short string describing the field.  It will
  be displayed to Bugzilla users in several parts of Bugzilla's UI,
  for example as the label for the field on the "show bug" page.

Warning:

  Custom fields can make Bugzilla less usable.  See this URL
  for alternatives to custom fields:
  
  http://www.gerv.net/hacking/custom-fields.html
  
  You should try to implement applicable alternatives before using
  this script to add a custom field.
END

    exit;
}

# Prepend cf_ to the custom field name to distinguish it from standard fields.
$name =~ /^cf_/
  or $name = "cf_" . $name;

# Exit gracefully if there is already a field with the given name.
if (scalar(Bugzilla::Field::match({ name=>$name })) > 0) {
    print "There is already a field named $name.  Please choose " .
          "a different name.\n";
    exit;
}


# Create the field.
print "Creating custom field $name ...\n";
my $field = Bugzilla::Field::create($name, $desc, 1);
print "Custom field $name created.\n";
