#!/usr/bin/perl -w
# -*- mode: cperl; c-basic-offset: 8; indent-tabs-mode: nil; -*-

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Litmus.
#
# The Initial Developer of the Original Code is
# the Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Chris Cooper <ccooper@deadsquid.com>
#   Zach Lipton <zach@zachlipton.com>
#
# ***** END LICENSE BLOCK *****

use strict;

# Litmus homepage

use Litmus;
use Litmus::Auth;
use Litmus::Error;
use Litmus::DB::Testresult;
use Litmus::FormWidget;

use CGI;
use Time::Piece::MySQL;

my $c = new CGI; 
print $c->header();

use diagnostics;

# Hash refs for maintaining state in the search form.
my $defaults;
my $order_bys;

our $MAX_SORT_FIELDS = 10;
our $MAX_SEARCH_FIELDS = 10;

my $criteria = "Custom<br/>";
my $results;
my @where;
my @order_by;
my $limit;
my $where_criteria = "";
my $order_by_criteria = "";
my $limit_criteria = "";
if ($c->param) {    

    foreach my $param ($c->param) {
        next if ($c->param($param) eq '');
        
        if ($param =~ /sort_field(\d+)/) {
            # We slot sort fields into the @order_by array based on their
            # field_num. Empty array slots will be ignored when the SQL 
            # is built. We set an upper limit on the number of sort fields 
            # we can handle to prevent abuse.
            my $field_num = $1;
            next if ($field_num > $MAX_SORT_FIELDS);
            my $sort_field = $c->param($param);
            my $sort_order = 'ASC';
            if ($c->param("sort_order$field_num")) {
                $sort_order = $c->param("sort_order$field_num");
            }
            $order_by[$field_num] = { field => $sort_field,
                                      direction => $sort_order};

        } elsif ($param =~ /search_field(\d+)/) {
            # We set an upper limit on the number of search fields 
            # we can handle to prevent abuse.
            my $field_num = $1;
            next if ($field_num > $MAX_SEARCH_FIELDS);
            my $search_field = $c->param($param);
            my $match_criteria = $c->param("match_criteria$field_num");
            my $value = $c->param("search_value$field_num");
            push @where, { 'field' => 'search_field',
                           'search_field' => $search_field,
                           'match_criteria' => $match_criteria,
                           'value' => $value};

            $where_criteria .= "$search_field $match_criteria '$value'<br/>";

        } elsif ($param eq 'start_date') {
            my $start_date = $c->param($param);
            $start_date =~ s/[^0-9A-Za-z ]/ /g;
            my $end_date;
            # Use 'now' as the default end date.
            if ($c->param('end_date') and $c->param('end_date') ne '') {
                $end_date = $c->param('end_date');
                $end_date =~ s/[^0-9A-Za-z ]/ /g;
            } else {
                $end_date = 'Now';
            }
            push @where, { field => 'start_date',
                           value => $start_date};
            push @where, { field => 'end_date',
                           value => $end_date};
            $where_criteria .= "Date between '$start_date' and '$end_date'<br/>";
        } elsif ($param eq 'trusted_only') {
            push @where, {field => 'trusted_only',
                          value => 1};            
            $limit_criteria .= "Display trusted results only<br/>";
        } elsif ($param eq "limit") {
            $limit = $c->param($param);
            next if ($limit == $Litmus::DB::Testresult::_num_results_default);
            $limit_criteria .= "Limit to $limit results<br/>";
        } elsif ($param eq 'branch') {
            my $value = $c->param($param);
            push @where, {field => $param,
                          value => $value};
            $where_criteria .= "Branch is \'".$c->param($param)."\'<br/>";
            $defaults->{branch} =  $c->param($param);
        } elsif ($param eq 'product') {
            my $value = $c->param($param);
            push @where, {field => $param,
                          value => $value};
            $where_criteria .= "Product is \'".$c->param($param)."\'<br/>";
            $defaults->{product} = $c->param($param);
        } elsif ($param eq 'platform') {
            my $value = $c->param($param);
            push @where, {field => $param,
                          value => $value};
            $where_criteria .= "Platform is \'".$c->param($param)."\'<br/>";
            $defaults->{platform} = $c->param($param);
        } elsif ($param eq 'test_group') {
            my $value = $c->param($param);
            push @where, {field => $param,
                          value => $value};
            $where_criteria .= "Test group is \'".$c->param($param)."\'<br/>";
            $defaults->{test_group} = $c->param($param);
        } elsif ($param eq 'test_id') {
            my $value = $c->param($param);
            push @where, {field => $param,
                          value => $value};
            $where_criteria .= "Testcase ID# is \'".$c->param($param)."\'<br/>";
            $defaults->{test_id} = $c->param($param);                
        } elsif ($param eq 'summary') {
            my $value = $c->param($param);
            push @where, {field => $param,
                          value => $value};
            $where_criteria .= "Summary like \'".$c->param($param)."\'<br/>";
            $defaults->{summary} = $c->param($param);
        } elsif ($param eq 'result_status') {
            my $value = $c->param($param);
            push @where, {field => $param,
                          value => $value};
            $where_criteria .= "Status is \'".$c->param($param)."\'<br/>";
            $defaults->{result_status} = $c->param($param);
        } else {
            # Skip unknown field
        }
    }
    if ($where_criteria eq '' and 
        scalar(@order_by) == 0 and
        $limit_criteria eq '') {
        ($criteria,$results) = 
          Litmus::DB::Testresult->getDefaultTestResults;    
    } else {
        foreach my $order_by_field (@order_by) {
            next if (!$order_by_field);
            $order_by_criteria .= "Order by $order_by_field->{field} $order_by_field->{direction}<br/>";
        }

        $criteria .= $where_criteria . $order_by_criteria . $limit_criteria;
        $criteria =~ s/_/ /g;
        $results = Litmus::DB::Testresult->getTestResults(\@where,
                                                          \@order_by,
                                                          $limit);
    }
} else {
    ($criteria,$results) = 
      Litmus::DB::Testresult->getDefaultTestResults;    
}

# Populate each of our form widgets for select/input.
# Set a default value as appropriate.
my $products = Litmus::FormWidget->getProducts;
my $platforms = Litmus::FormWidget->getPlatforms;
my $test_groups = Litmus::FormWidget->getTestGroups;
my $test_ids = Litmus::FormWidget->getTestIDs;
my $result_statuses = Litmus::FormWidget->getResultStatuses;
my $branches = Litmus::FormWidget->getBranches;

my $fields = Litmus::FormWidget->getFields;
my $match_criteria = Litmus::FormWidget->getMatchCriteria;
my $sort_fields = Litmus::FormWidget->getSortFields;

my $title = 'Advanced Search';

my $vars = {
    title => $title,
    criteria => $criteria,
    products => $products,
    platforms => $platforms,
    test_groups => $test_groups,
    test_ids => $test_ids,
    result_statuses => $result_statuses,
    branches => $branches,
    fields => $fields,
    match_criteria => $match_criteria,
    sort_fields => $sort_fields,
};

# Only include results if we have them.
if ($results and scalar @$results > 0) {
    $vars->{results} = $results;
}

my $cookie =  Litmus::Auth::getCookie();
$vars->{"defaultemail"} = $cookie;
$vars->{"show_admin"} = Litmus::Auth::istrusted($cookie);

Litmus->template()->process("reporting/advanced_search.tmpl", $vars) || 
    internalError(Litmus->template()->error());

exit 0;










