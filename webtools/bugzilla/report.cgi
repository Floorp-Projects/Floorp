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

use diagnostics;
use strict;
use lib ".";

require "CGI.pl";

use vars qw($template $vars);

use Bugzilla::Search;

ConnectToDatabase();

GetVersionTable();

quietly_check_login();

# If other report types are added, some of this code can be moved into a sub,
# and the correct sub chosen based on $::FORM{'type'}.

$::FORM{'y_axis_field'} || ThrowCodeError("no_y_axis_defined");

my $col_field = $::FORM{'x_axis_field'};
my $row_field = $::FORM{'y_axis_field'};

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

my @axis_fields = ($row_field);
# The X axis (horizontal) is optional
push(@axis_fields, $col_field) if $col_field;

my @selectnames = map($columns{$_}, @axis_fields);

my $search = new Bugzilla::Search('fields' => \@selectnames, 
                                  'url' => $::buffer);
my $query = $search->getSQL();

$query =~ s/DISTINCT//;
SendSQL($query, $::userid);

# We have a hash for each direction for the totals, and a hash of hashes for 
# the data itself.
my %data;
my %row_totals;
my %col_totals;
my $grand_total;

# Read the bug data and increment the counts.
while (MoreSQLData()) {
    my ($row, $col) = FetchSQLData();    
    $row = "" if !defined($row);
    $col = "" if !defined($col);
    
    $data{$row}{$col}++;    
    $row_totals{$row}++;    
    $col_totals{$col}++;    
    $grand_total++;
}

$vars->{'data'} = \%data;
$vars->{'row_totals'} = \%row_totals;
$vars->{'col_totals'} = \%col_totals;
$vars->{'grand_total'} = $grand_total;

# Determine the labels for the rows and columns
my @row_names = sort(keys(%row_totals));
my @col_names = sort(keys(%col_totals));

$vars->{'row_names'} = \@row_names;
$vars->{'col_names'} = \@col_names;

$vars->{'row_field'} = $row_field;
$vars->{'col_field'} = $col_field;

$::buffer =~ s/format=[^&]*&?//g;

# Calculate the base query URL for the hyperlinked numbers
my $buglistbase = $::buffer;
$buglistbase =~ s/$row_field=[^&]*&?//g;
$buglistbase =~ s/$col_field=[^&]*&?//g;

$vars->{'buglistbase'} = $buglistbase;
$vars->{'buffer'} = $::buffer;

$::FORM{'type'} =~ s/[^a-zA-Z\-]//g;

# Generate and return the result from the appropriate template.
my $format = GetFormat("reports/$::FORM{'type'}", 
                       $::FORM{'format'}, 
                       $::FORM{'ctype'});
print "Content-Type: $format->{'contenttype'}\n\n";
$template->process("$format->{'template'}", $vars)
  || ThrowTemplateError($template->error());
