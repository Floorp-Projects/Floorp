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
# Contributor(s): Gervase Markham <gerv@gerv.net>
#                 <rdean@cambianetworks.com>

use strict;
use lib ".";

require "CGI.pl";

use vars qw($template $vars);

use Bugzilla::Search;

ConnectToDatabase();

GetVersionTable();

quietly_check_login();

if ($::FORM{'action'} ne "plot") {
    print "Content-Type: text/html\n\n";
    $template->process("reports/menu.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

$::FORM{'y_axis_field'} || ThrowCodeError("no_y_axis_defined");

if ($::FORM{'z_axis_field'} && !$::FORM{'x_axis_field'}) {
    ThrowUserError("z_axis_defined_with_no_x_axis");
}

my $col_field = $::FORM{'x_axis_field'};
my $row_field = $::FORM{'y_axis_field'};
my $tbl_field = $::FORM{'z_axis_field'};

my %columns;
$columns{'bug_severity'}     = "bugs.bug_severity";        
$columns{'priority'}         = "bugs.priority";
$columns{'rep_platform'}     = "bugs.rep_platform";
$columns{'assigned_to'}      = "map_assigned_to.login_name";
$columns{'reporter'}         = "map_reporter.login_name";
$columns{'qa_contact'}       = "map_qa_contact.login_name";
$columns{'bug_status'}       = "bugs.bug_status";
$columns{'resolution'}       = "bugs.resolution";
$columns{'component'}        = "map_components.name";
$columns{'product'}          = "map_products.name";
$columns{'version'}          = "bugs.version";
$columns{'op_sys'}           = "bugs.op_sys";
$columns{'votes'}            = "bugs.votes";
$columns{'keywords'}         = "bugs.keywords";
$columns{'target_milestone'} = "bugs.target_milestone";
# One which means "nothing". Any number would do, really. It just gets SELECTed
# so that we always select 3 items in the query.
$columns{''}                 = "42217354";

my @axis_fields = ($row_field, $col_field, $tbl_field);

my @selectnames = map($columns{$_}, @axis_fields);

my $search = new Bugzilla::Search('fields' => \@selectnames, 
                                  'url' => $::buffer);
my $query = $search->getSQL();

SendSQL($query, $::userid);

# We have a hash of hashes for the data itself, and a hash to hold the 
# row/col/table names.
my %data;
my %names;

# Read the bug data and increment the counts.
while (MoreSQLData()) {
    my ($row, $col, $tbl) = FetchSQLData();
    $col = "" if ($col eq $columns{''});
    $tbl = "" if ($tbl eq $columns{''});
    
    $data{$tbl}{$col}{$row}++;
    $names{"col"}{$col}++;
    $names{"row"}{$row}++;
    $names{"tbl"}{$tbl}++;
}

# Determine the labels for the rows and columns
$vars->{'col_field'} = $col_field;
$vars->{'row_field'} = $row_field;
$vars->{'tbl_field'} = $tbl_field;
$vars->{'names'} = \%names;
$vars->{'data'} = \%data;
$vars->{'time'} = time();

$::buffer =~ s/format=[^&]*&?//g;

# Calculate the base query URL for the hyperlinked numbers
$vars->{'buglistbase'} = CanonicaliseParams($::buffer, 
                ["x_axis_field", "y_axis_field", "z_axis_field", @axis_fields]);
$vars->{'buffer'} = $::buffer;

# Generate and return the result from the appropriate template.
my $format = GetFormat("reports/report", $::FORM{'format'}, $::FORM{'ctype'});
print "Content-Type: $format->{'ctype'}\n\n";
$template->process("$format->{'template'}", $vars)
  || ThrowTemplateError($template->error());
