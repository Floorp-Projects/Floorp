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
# Module Initialization
################################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;

# This module implements flag types for the flag tracker.
package Bugzilla::FlagType;

# Use Bugzilla's User module which contains utilities for handling users.
use Bugzilla::User;

use Bugzilla::Error;
use Bugzilla::Util;
use Bugzilla::Config;

# Note!  This module requires that its caller have said "require CGI.pl" 
# to import relevant functions from that script and its companion globals.pl.

################################################################################
# Global Variables
################################################################################

# basic sets of columns and tables for getting flag types from the database

my @base_columns = 
  ("1", "flagtypes.id", "flagtypes.name", "flagtypes.description", 
   "flagtypes.cc_list", "flagtypes.target_type", "flagtypes.sortkey", 
   "flagtypes.is_active", "flagtypes.is_requestable", 
   "flagtypes.is_requesteeble", "flagtypes.is_multiplicable");

# Note: when adding tables to @base_tables, make sure to include the separator 
# (i.e. a comma or words like "LEFT OUTER JOIN") before the table name, 
# since tables take multiple separators based on the join type, and therefore 
# it is not possible to join them later using a single known separator.

my @base_tables = ("flagtypes");

################################################################################
# Public Functions
################################################################################

sub get {
    # Returns a hash of information about a flag type.

    my ($id) = @_;

    my $select_clause = "SELECT " . join(", ", @base_columns);
    my $from_clause = "FROM " . join(" ", @base_tables);
    
    &::PushGlobalSQLState();
    &::SendSQL("$select_clause $from_clause WHERE flagtypes.id = $id");
    my @data = &::FetchSQLData();
    my $type = perlify_record(@data);
    &::PopGlobalSQLState();

    return $type;
}

sub get_inclusions {
    my ($id) = @_;
    return get_clusions($id, "in");
}

sub get_exclusions {
    my ($id) = @_;
    return get_clusions($id, "ex");
}

sub get_clusions {
    my ($id, $type) = @_;
    
    &::PushGlobalSQLState();
    &::SendSQL("SELECT products.name, components.name " . 
               "FROM flagtypes, flag${type}clusions " . 
               "LEFT OUTER JOIN products ON flag${type}clusions.product_id = products.id " . 
               "LEFT OUTER JOIN components ON flag${type}clusions.component_id = components.id " . 
               "WHERE flagtypes.id = $id AND flag${type}clusions.type_id = flagtypes.id");
    my @clusions = ();
    while (&::MoreSQLData()) {
        my ($product, $component) = &::FetchSQLData();
        $product ||= "Any";
        $component ||= "Any";
        push(@clusions, "$product:$component");
    }
    &::PopGlobalSQLState();
    
    return \@clusions;
}

sub match {
    # Queries the database for flag types matching the given criteria
    # and returns the set of matching types.

    my ($criteria, $include_count) = @_;

    my @tables = @base_tables;
    my @columns = @base_columns;
    my $having = "";
    
    # Include a count of the number of flags per type if requested.
    if ($include_count) { 
        push(@columns, "COUNT(flags.id)");
        push(@tables, "LEFT OUTER JOIN flags ON flagtypes.id = flags.type_id");
    }
    
    # Generate the SQL WHERE criteria.
    my @criteria = sqlify_criteria($criteria, \@tables, \@columns, \$having);
    
    # Build the query, grouping the types if we are counting flags.
    my $select_clause = "SELECT " . join(", ", @columns);
    my $from_clause = "FROM " . join(" ", @tables);
    my $where_clause = "WHERE " . join(" AND ", @criteria);
    
    my $query = "$select_clause $from_clause $where_clause";
    $query .= " GROUP BY flagtypes.id " if ($include_count || $having ne "");
    $query .= " HAVING $having " if $having ne "";
    $query .= " ORDER BY flagtypes.sortkey, flagtypes.name";
    
    # Execute the query and retrieve the results.
    &::PushGlobalSQLState();
    &::SendSQL($query);
    my @types;
    while (&::MoreSQLData()) {
        my @data = &::FetchSQLData();
        my $type = perlify_record(@data);
        push(@types, $type);
    }
    &::PopGlobalSQLState();

    return \@types;
}

sub count {
    # Returns the total number of flag types matching the given criteria.
    
    my ($criteria) = @_;

    # Generate query components.
    my @tables = @base_tables;
    my @columns = ("COUNT(flagtypes.id)");
    my $having = "";
    my @criteria = sqlify_criteria($criteria, \@tables, \@columns, \$having);
    
    # Build the query.
    my $select_clause = "SELECT " . join(", ", @columns);
    my $from_clause = "FROM " . join(" ", @tables);
    my $where_clause = "WHERE " . join(" AND ", @criteria);
    my $query = "$select_clause $from_clause $where_clause";
    $query .= " GROUP BY flagtypes.id HAVING $having " if $having ne "";
        
    # Execute the query and get the results.
    &::PushGlobalSQLState();
    &::SendSQL($query);
    my $count = &::FetchOneColumn();
    &::PopGlobalSQLState();

    return $count;
}

sub validate {
    my ($data, $bug_id, $attach_id) = @_;
  
    # Get a list of flag types to validate.  Uses the "map" function
    # to extract flag type IDs from form field names by matching columns
    # whose name looks like "flag_type-nnn", where "nnn" is the ID,
    # and returning just the ID portion of matching field names.
    my @ids = map(/^flag_type-(\d+)$/ ? $1 : (), keys %$data);
  
    foreach my $id (@ids)
    {
        my $status = $data->{"flag_type-$id"};
        
        # Don't bother validating types the user didn't touch.
        next if $status eq "X";

        # Make sure the flag type exists.
        my $flag_type = get($id);
        $flag_type 
          || ThrowCodeError("flag_type_nonexistent", { id => $id });

        # Make sure the value of the field is a valid status.
        grep($status eq $_, qw(X + - ?))
          || ThrowCodeError("flag_status_invalid", 
                            { id => $id , status => $status });

        # Make sure the user didn't request the flag unless it's requestable.
        if ($status eq '?' && !$flag_type->{is_requestable}) {
            ThrowCodeError("flag_status_invalid", 
                           { id => $id , status => $status });
        }
        
        # Make sure the requestee is authorized to access the bug
        # (and attachment, if this installation is using the "insider group"
        # feature and the attachment is marked private).
        if ($status eq '?'
            && $flag_type->{is_requesteeble}
            && trim($data->{"requestee_type-$id"}))
        {
            my $requestee_email = trim($data->{"requestee_type-$id"});

            # We know the requestee exists because we ran
            # Bugzilla::User::match_field before getting here.
            my $requestee = Bugzilla::User->new_from_login($requestee_email);

            # Throw an error if the user can't see the bug.
            if (!$requestee->can_see_bug($bug_id))
            {
                ThrowUserError("flag_requestee_unauthorized",
                               { flag_type => $flag_type,
                                 requestee => $requestee,
                                 bug_id    => $bug_id,
                                 attach_id => $attach_id });
            }
            
            # Throw an error if the target is a private attachment and
            # the requestee isn't in the group of insiders who can see it.
            if ($attach_id
                && Param("insidergroup")
                && $data->{'isprivate'}
                && !$requestee->in_group(Param("insidergroup")))
            {
                ThrowUserError("flag_requestee_unauthorized_attachment",
                               { flag_type => $flag_type,
                                 requestee => $requestee,
                                 bug_id    => $bug_id,
                                 attach_id => $attach_id });
            }
        }
    }
}

sub normalize {
    # Given a list of flag types, checks its flags to make sure they should
    # still exist after a change to the inclusions/exclusions lists.
    
    # A list of IDs of flag types to normalize.
    my (@ids) = @_;
    
    my $ids = join(", ", @ids);
    
    # Check for flags whose product/component is no longer included.
    &::SendSQL("
        SELECT flags.id 
        FROM (flags INNER JOIN bugs ON flags.bug_id = bugs.bug_id)
          LEFT OUTER JOIN flaginclusions AS i
            ON (flags.type_id = i.type_id
            AND (bugs.product_id = i.product_id OR i.product_id IS NULL)
            AND (bugs.component_id = i.component_id OR i.component_id IS NULL))
        WHERE flags.type_id IN ($ids)
        AND flags.is_active = 1
        AND i.type_id IS NULL
    ");
    Bugzilla::Flag::clear(&::FetchOneColumn()) while &::MoreSQLData();
    
    &::SendSQL("
        SELECT flags.id 
        FROM flags, bugs, flagexclusions AS e
        WHERE flags.type_id IN ($ids)
        AND flags.bug_id = bugs.bug_id
        AND flags.type_id = e.type_id 
        AND flags.is_active = 1
        AND (bugs.product_id = e.product_id OR e.product_id IS NULL)
        AND (bugs.component_id = e.component_id OR e.component_id IS NULL)
    ");
    Bugzilla::Flag::clear(&::FetchOneColumn()) while &::MoreSQLData();
}

################################################################################
# Private Functions
################################################################################

sub sqlify_criteria {
    # Converts a hash of criteria into a list of SQL criteria.
    # $criteria is a reference to the criteria (field => value), 
    # $tables is a reference to an array of tables being accessed 
    # by the query, $columns is a reference to an array of columns
    # being returned by the query, and $having is a reference to
    # a criterion to put into the HAVING clause.
    my ($criteria, $tables, $columns, $having) = @_;

    # the generated list of SQL criteria; "1=1" is a clever way of making sure
    # there's something in the list so calling code doesn't have to check list
    # size before building a WHERE clause out of it
    my @criteria = ("1=1");

    if ($criteria->{name}) {
        push(@criteria, "flagtypes.name = " . &::SqlQuote($criteria->{name}));
    }
    if ($criteria->{target_type}) {
        # The target type is stored in the database as a one-character string
        # ("a" for attachment and "b" for bug), but this function takes complete
        # names ("attachment" and "bug") for clarity, so we must convert them.
        my $target_type = &::SqlQuote(substr($criteria->{target_type}, 0, 1));
        push(@criteria, "flagtypes.target_type = $target_type");
    }
    if (exists($criteria->{is_active})) {
        my $is_active = $criteria->{is_active} ? "1" : "0";
        push(@criteria, "flagtypes.is_active = $is_active");
    }
    if ($criteria->{product_id} && $criteria->{'component_id'}) {
        my $product_id = $criteria->{product_id};
        my $component_id = $criteria->{component_id};
        
        # Add inclusions to the query, which simply involves joining the table
        # by flag type ID and target product/component.
        push(@$tables, "INNER JOIN flaginclusions ON " .
                       "flagtypes.id = flaginclusions.type_id");
        push(@criteria, "(flaginclusions.product_id = $product_id " . 
                        " OR flaginclusions.product_id IS NULL)");
        push(@criteria, "(flaginclusions.component_id = $component_id " . 
                        " OR flaginclusions.component_id IS NULL)");
        
        # Add exclusions to the query, which is more complicated.  First of all,
        # we do a LEFT JOIN so we don't miss flag types with no exclusions.
        # Then, as with inclusions, we join on flag type ID and target product/
        # component.  However, since we want flag types that *aren't* on the
        # exclusions list, we count the number of exclusions records returned
        # and use a HAVING clause to weed out types with one or more exclusions.
        my $join_clause = "flagtypes.id = flagexclusions.type_id " . 
                          "AND (flagexclusions.product_id = $product_id " . 
                          "OR flagexclusions.product_id IS NULL) " . 
                          "AND (flagexclusions.component_id = $component_id " .
                          "OR flagexclusions.component_id IS NULL)";
        push(@$tables, "LEFT JOIN flagexclusions ON ($join_clause)");
        push(@$columns, "COUNT(flagexclusions.type_id) AS num_exclusions");
        $$having = "num_exclusions = 0";
    }
    
    return @criteria;
}

sub perlify_record {
    # Converts data retrieved from the database into a Perl record.
    
    my $type = {};
    
    $type->{'exists'} = $_[0];
    $type->{'id'} = $_[1];
    $type->{'name'} = $_[2];
    $type->{'description'} = $_[3];
    $type->{'cc_list'} = $_[4];
    $type->{'target_type'} = $_[5] eq "b" ? "bug" : "attachment";
    $type->{'sortkey'} = $_[6];
    $type->{'is_active'} = $_[7];
    $type->{'is_requestable'} = $_[8];
    $type->{'is_requesteeble'} = $_[9];
    $type->{'is_multiplicable'} = $_[10];
    $type->{'flag_count'} = $_[11];
        
    return $type;
}

1;
