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
# The Original Code is the Bugzilla Test Runner System.
#
# The Initial Developer of the Original Code is Maciej Maczynski.
# Portions created by Maciej Maczynski are Copyright (C) 2001
# Maciej Maczynski. All Rights Reserved.
#
# Contributor(s): Maciej Maczynski <macmac@xdsnet.pl>
#                 Ed Fuentetaja <efuentetaja@acm.org>
#                 Greg Hendricks <ghendricks@novell.com>

=head1 NAME

Bugzilla::Testopia::Util

=head1 DESCRIPTION

This module contains miscillaneous functions used by Testopia. 
It exports all of these functions using Exporter.

=cut

package Bugzilla::Testopia::Util;

use strict;

use base qw(Exporter);
@Bugzilla::Testopia::Util::EXPORT = qw(get_product_plans get_product_components 
                                         can_view_product GetProducts
                                         get_field_id get_time_stamp 
                                         validate_test_id validate_selection
                                         validate_version);

use Bugzilla;
use Bugzilla::Config;
use Bugzilla::Error;
use Bugzilla::Constants;
use Bugzilla::Util;
use Bugzilla::Testopia::TestPlan;

### Methods ###

sub get_product_plans {
    my ($product_id) = @_;
    my $dbh = Bugzilla->dbh;
    my @testplans;
    
    my $col = $dbh->selectcol_arrayref("SELECT plan_id 
                                        FROM test_plans 
                                        WHERE product_id=?", 
                                        undef,$product_id);
    foreach my $plan_id (@{$col}){
        push @testplans, Bugzilla::Testopia::TestPlan->new($plan_id);
    }
    return \@testplans;
}

sub get_product_components {
    my ($product_id) = @_;
    my $dbh = Bugzilla->dbh;
    
    my $components = $dbh->selectcol_arrayref("SELECT name FROM components 
                                               WHERE product_id=?", undef, $product_id);
    return $components;
}
=head2 can_view_product

Returns true if the logged in user has rights to edit bugs and
by extension, test plans in a prodcut.

=cut

# Code adapted from GetSelectableProducts:
sub can_view_product {
    my $dbh = Bugzilla->dbh;
    my ($product_id) = @_;
    
    my $query = "SELECT 1 " .
                  "FROM products " .
              "LEFT JOIN group_control_map " .
              "ON group_control_map.product_id = products.id ";
    if (Param('useentrygroupdefault')) {
        $query .= "AND group_control_map.entry != 0 ";
    } else {
        $query .= "AND group_control_map.membercontrol = " .
              CONTROLMAPMANDATORY . " ";
    }
    if (%{Bugzilla->user->groups}) {
        $query .= "AND group_id NOT IN(" . 
              join(',', values(%{Bugzilla->user->groups})) . ") ";
    }
    $query .= "WHERE group_id IS NULL AND products.id= ?";

    my ($canedit) = $dbh->selectrow_array($query,undef,$product_id);
    
    return $canedit;
}

=head2 get_field_id

Takes a field name and table and returns the fieldid from the 
test_fielddefs table.

=cut

sub get_field_id {
    my ($field, $table) = @_;
    my $dbh = Bugzilla->dbh;

    my ($field_id) = $dbh->selectrow_array(
             "SELECT fieldid 
              FROM test_fielddefs 
              WHERE name = ? AND table_name = ?",
              undef, $field, $table);
    return $field_id;
}

=head2 get_time_stamp

Returns an SQL timestamp

=cut

sub get_time_stamp {
   my $dbh = Bugzilla->dbh;
   my ($timestamp) = $dbh->selectrow_array("SELECT NOW()");
   return $timestamp;
}

=head2 tc_alias_to_id

Takes a test case alias and returns the corresponding ID

=cut

sub tc_alias_to_id {
    my ($alias) = @_;
    my $dbh = Bugzilla->dbh;
    trick_taint($alias);
    return $dbh->selectrow_array(
        "SELECT case_id FROM test_cases WHERE alias = ?", undef, $alias);
}

=head2 validate_test_id

Takes an ID and a Testopia type and validates it against the database.
In the case of cases it will validate and return an ID from an alias.
Much of this was taken from Bugzilla. This function assumes all tables
in the database are named test_<object>s

=cut

sub validate_test_id {
    my $dbh = Bugzilla->dbh;
    my ($id, $type) = @_;
    $id = trim($id);
    
    # If the ID isn't a number, it might be an alias, so try to convert it.
    my $alias = $id;
    if (!detaint_natural($id) && $type eq 'case') {
        $id = tc_alias_to_id($alias);
        $id || ThrowUserError("testopia-invalid-test-id-or-alias",
                              {'case_id' => $alias});
    }
    
    # Modify the calling code's original variable to contain the trimmed,
    # converted-from-alias ID.
    $_[0] = $id;
    
    # First check that the object exists
    my ($res) = $dbh->selectrow_array("SELECT ". $type. "_id FROM test_". $type."s 
                                       WHERE ". $type ."_id=?",undef, $id);

    $res
      || ThrowUserError("invalid-test-id-non-existent", 
                            {'id' => $id, 'type' => $type});
    return $res;
}

=head2 validate_selection

Checks that the selected option is a valid one

=cut

sub validate_selection {
    my $dbh = Bugzilla->dbh;
    my ($id, $field, $table) = @_;
    $id = trim($id);
    
    # First check that the object exists taint check should have been 
    # done before calling this function.
    my ($res) = $dbh->selectrow_array(
        "SELECT $field 
           FROM $table
          WHERE $field = ?",
          undef, $id);

    $res
      || ThrowUserError("invalid-test-id-non-existent", 
                            {'id' => $id, 'type' => $table});
    return $res;
}

sub validate_version {
    my $dbh = Bugzilla->dbh;
    my ($version, $product) = @_;
    
    # First check that the object exists. 
    # Taint check should have been done before calling this function.
    my ($res) = $dbh->selectrow_array(
        "SELECT 1 
           FROM versions
          WHERE product_id = ?",
          undef, $product);

    # If the version does not match the product return the first one
    # that does
    unless ($res){
        ($res) = $dbh->selectrow_array(
        "SELECT value 
           FROM versions
          WHERE product_id = ?",
          undef, $product);
    }
    
    return $res;
}

=head1 AUTHOR

Greg Hendricks <ghendricks@novell.com>

=cut

1;
