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

use strict;
use lib ".";

# This module implements a series - a set of data to be plotted on a chart.
package Bugzilla::Series;

use Bugzilla;
use Bugzilla::Util;
use Bugzilla::User;

sub new {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
  
    # Create a ref to an empty hash and bless it
    my $self = {};
    bless($self, $class);

    if ($#_ == 0) {
        if (ref($_[0])) {
            # We've been given a CGI object
            $self->readParametersFromCGI($_[0]);
            $self->createInDatabase();
        }
        else {
            # We've been given a series_id.
            $self->initFromDatabase($_[0]);
        }
    }
    elsif ($#_ >= 3) {
        $self->initFromParameters(@_);
    }
    else {
        die("Bad parameters passed in - invalid number of args \($#_\)($_)");
    }

    return $self->{'already_exists'} ? $self->{'series_id'} : $self;
}

sub initFromDatabase {
    my $self = shift;
    my $series_id = shift;
    
    &::detaint_natural($series_id) 
      || &::ThrowCodeError("invalid_series_id", { 'series_id' => $series_id });
    
    my $dbh = Bugzilla->dbh;
    my @series = $dbh->selectrow_array("SELECT series.series_id, cc1.name, " .
        "cc2.name, series.name, series.creator, series.frequency, " .
        "series.query " .
        "FROM series " .
        "LEFT JOIN series_categories AS cc1 " .
        "    ON series.category = cc1.category_id " .
        "LEFT JOIN series_categories AS cc2 " .
        "    ON series.subcategory = cc2.category_id " .
        "WHERE series.series_id = $series_id");
    
    if (@series) {
        $self->initFromParameters(@series);
    }
    else {
        &::ThrowCodeError("invalid_series_id", { 'series_id' => $series_id });
    }
}

sub initFromParameters {
    my $self = shift;

    # The first four parameters are compulsory, unless you immediately call
    # createInDatabase(), in which case series_id can be left off.
    ($self->{'series_id'}, $self->{'category'},  $self->{'subcategory'},
     $self->{'name'}, $self->{'creator'}, $self->{'frequency'},
                                                        $self->{'query'}) = @_;

    $self->{'public'} = $self->isSubscribed(0);
    $self->{'subscribed'} = $self->isSubscribed($::userid);
}

sub createInDatabase {
    my $self = shift;

    # Lock some tables
    my $dbh = Bugzilla->dbh;
    $dbh->do("LOCK TABLES series_categories WRITE, series WRITE, " .
             "user_series_map WRITE");

    my $category_id = getCategoryID($self->{'category'});
    my $subcategory_id = getCategoryID($self->{'subcategory'});

    $self->{'creator'} = $::userid;

    # Check for the series currently existing
    trick_taint($self->{'name'});
    $self->{'series_id'} = $dbh->selectrow_array("SELECT series_id " .
                              "FROM series WHERE category = $category_id " .
                              "AND subcategory = $subcategory_id AND name = " .
                              $dbh->quote($self->{'name'}));

    if ($self->{'series_id'}) {
        $self->{'already_exists'} = 1;
    }
    else {
        trick_taint($self->{'query'});

        # Insert the new series into the series table
        $dbh->do("INSERT INTO series (creator, category, subcategory, " .
                 "name, frequency, query) VALUES ($self->{'creator'}, " .
                 "$category_id, $subcategory_id, " .
                 $dbh->quote($self->{'name'}) . ", $self->{'frequency'}," .
                 $dbh->quote($self->{'query'}) . ")");

        # Retrieve series_id
        $self->{'series_id'} = $dbh->selectrow_array("SELECT MAX(series_id) " .
                                                     "FROM series");
        $self->{'series_id'}
          || &::ThrowCodeError("missing_series_id", { 'series' => $self });

        # Subscribe user to the newly-created series.
        $self->subscribe($::userid);
        # Public series are subscribed to by userid 0.
        $self->subscribe(0) if ($self->{'public'} && $::userid != 0);
    }

    $dbh->do("UNLOCK TABLES");
}

# Get a category or subcategory IDs, creating the category if it doesn't exist.
sub getCategoryID {
    my ($category) = @_;
    my $category_id;
    my $dbh = Bugzilla->dbh;

    # This seems for the best idiom for "Do A. Then maybe do B and A again."
    while (1) {
        # We are quoting this to put it in the DB, so we can remove taint
        trick_taint($category);

        $category_id = $dbh->selectrow_array("SELECT category_id " .
                                      "from series_categories " .
                                      "WHERE name =" . $dbh->quote($category));
        last if $category_id;

        $dbh->do("INSERT INTO series_categories (name) " .
                 "VALUES (" . $dbh->quote($category) . ")");
    }

    return $category_id;
}        

sub readParametersFromCGI {
    my $self = shift;
    my $cgi = shift;

    $self->{'category'} = $cgi->param('category')
      || $cgi->param('newcategory')
      || &::ThrowUserError("missing_category");

    $self->{'subcategory'} = $cgi->param('subcategory')
      || $cgi->param('newsubcategory')
      || &::ThrowUserError("missing_subcategory");

    $self->{'name'} = $cgi->param('name')
      || &::ThrowUserError("missing_name");

    $self->{'frequency'} = $cgi->param('frequency');
    detaint_natural($self->{'frequency'})
      || &::ThrowUserError("missing_frequency");

    $self->{'public'} = $cgi->param('public') ? 1 : 0;
    
    $self->{'query'} = $cgi->canonicalise_query("format", "ctype", "action",
                                        "category", "subcategory", "name",
                                        "frequency", "public", "query_format");
}

sub alter {
    my $self = shift;
    my $cgi = shift;

    my $old_public = $self->{'public'};
    
    # Note: $self->{'query'} will be meaningless after this call
    $self->readParametersFromCGI($cgi);

    my $category_id = getCategoryID($self->{'category'});
    my $subcategory_id = getCategoryID($self->{'subcategory'}); 
        
    # Update the entry   
    trick_taint($self->{'name'});
    my $dbh = Bugzilla->dbh;
    $dbh->do("UPDATE series SET " .
             "category = $category_id, subcategory = $subcategory_id " .
             ", name = " . $dbh->quote($self->{'name'}) .
             ", frequency = $self->{'frequency'} " .
             "WHERE series_id = $self->{'series_id'}");
    
    # Update the publicness of this query.        
    if ($old_public && !$self->{'public'}) {
        $self->unsubscribe(0);
    }
    elsif (!$old_public && $self->{'public'}) {
        $self->subscribe(0);
    }             
}

sub subscribe {
    my $self = shift;
    my $userid = shift;
    
    if (!$self->isSubscribed($userid)) {
        # Subscribe current user to series_id
        my $dbh = Bugzilla->dbh;
        $dbh->do("INSERT INTO user_series_map " .
                 "VALUES($userid, $self->{'series_id'})");
    }    
}

sub unsubscribe {
    my $self = shift;
    my $userid = shift;
    
    if ($self->isSubscribed($userid)) {
        # Remove current user's subscription to series_id
        my $dbh = Bugzilla->dbh;
        $dbh->do("DELETE FROM user_series_map " .
                "WHERE user_id = $userid AND series_id = $self->{'series_id'}");
    }        
}

sub isSubscribed {
    my $self = shift;
    my $userid = shift;
    
    my $dbh = Bugzilla->dbh;
    my $issubscribed = $dbh->selectrow_array("SELECT 1 FROM user_series_map " .
                                       "WHERE user_id = $userid " .
                                       "AND series_id = $self->{'series_id'}");
    return $issubscribed;
}

1;
