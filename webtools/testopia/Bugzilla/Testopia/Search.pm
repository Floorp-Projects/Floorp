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

# Large portions lifted uncerimoniously from Bugzilla::Search.pm
# Which is copyrighted by its respective copyright holders
# Many thanks to the geniouses that contributed to that work of art:
#                 Gervase Markham <gerv@gerv.net>
#                 Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Stephan Niemz <st.n@gmx.net>
#                 Andreas Franke <afranke@mathweb.org>
#                 Myk Melez <myk@mozilla.org>
#                 Michael Schindler <michael@compressconsult.com>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>
#
# Contributor(s): Greg Hendricks <ghendricks@novell.com>

=head1 NAME

Bugzilla::Testopia::Search - A module to support searches in Testopis

=head1 DESCRIPTION

Testopia::Search is based heavilly on Bugzilla::Search. It takes a 
CGI instance and parses its parameters to generate an SQL query that
can be used to get a result set from the database. The query is 
usually passed to Table.pm to execute and display the results.
Search.pm supports searching for all major objects in the Testopia
database, Cases, Plans, Runs and Case-runs. It supports sorting
on one column at a time in ascending order.

=head1 SYNOPSIS

 $search = Bugzilla::Testopia::Search($cgi);

=cut

package Bugzilla::Testopia::Search;

use strict;

use Bugzilla::Util;
use Bugzilla::User;
use Bugzilla::Config;
use Bugzilla::Error;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::TestCase;

sub new {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
  
    my $self = {};
    bless($self, $class);

    $self->init(@_);
 
    return $self;
}

sub init {
    my $self = shift;
    my $cgi = shift;
    my $fields = shift;
    my $user = $self->{'user'} || Bugzilla->user;
    $self->{'cgi'} = $cgi;
    $self->{'fields'} = $fields if $fields;
    my $debug = $cgi->param('debug') || 0;
    my $dbh = Bugzilla->dbh;
    print $cgi->header if $debug;
    if ($debug && !$cgi->{'final_separator'}){
        use Data::Dumper;
        print Dumper($cgi);
    }
    my $page = $cgi->param('page') || 0;
    detaint_natural($page) if $page;
    $page = undef if ($cgi->param('viewall'));
    my $pagesize = $cgi->param('pagesize') if $cgi->param('pagesize');
    detaint_natural($pagesize) if defined $pagesize;
    $pagesize ||= 25;
    
    my @specialchart;
    my @supptables;
    my @wherepart;
    my @having;
    my @groupby;
    my @andlist;
    my @orderby;
    my @inputorder;
    my @fields;
    my %specialorderjoin;
    my %chartfields;
    #my ($testergroup) = $dbh->selectrow_array("SELECT id FROM groups WHERE name = ?",undef, 'Testers');

# $chartid is the number of the current chart whose SQL we're constructing
# $row is the current row of the current chart

# names for table aliases are constructed using $chartid and $row
#   SELECT blah  FROM $table "$table_$chartid_$row" WHERE ....

# $f  = field of table in bug db (e.g. bug_id, reporter, etc)
# $ff = qualified field name (field name prefixed by table)
#       e.g. bugs_activity.bug_id
# $t  = type of query. e.g. "equal to", "changed after", case sensitive substr"
# $v  = value - value the user typed in to the form
# $q  = sanitized version of user input (SqlQuote($v))
# @supptables = Tables and/or table aliases used in query
# %suppseen   = A hash used to store all the tables in supptables to weed
#               out duplicates.
# @supplist   = A list used to accumulate all the JOIN clauses for each
#               chart to merge the ON sections of each.
# $suppstring = String which is pasted into query containing all table names    
    my $chartid;
    my $sequence = 0;
    my $f;
    my $ff;
    my $t;
    my $q;
    my $v;
    my $term;
    my %funcsbykey;
    my $type;
    
    my $obj = trim($cgi->param('current_tab')) || ThrowUserError('testopia-missing-parameter', {'param' => 'current_tab'});
    ThrowUserError('unknown-tab') if $obj !~ '^(case|plan|run|case_run|environment)$';
    trick_taint($obj);
    
    # If what we intend to do is generate a report, we need some tables
    # to map names to ids
    if ($fields){
        ## Cases ##
        if (grep(/map_categories/, @$fields)) {
            push @supptables, "INNER JOIN test_case_categories AS map_categories " .
                              "ON test_cases.category_id = map_categories.category_id";
        }
        if (grep(/map_priority/, @$fields)) {
            push @supptables, "INNER JOIN priority AS map_priority " .
                              "ON test_cases.priority_id = map_priority.id";
        }
        if (grep(/map_case_status/, @$fields)) {
            push @supptables, "INNER JOIN test_case_status AS map_case_status " .
                              "ON test_cases.case_status_id = map_case_status.case_status_id";
        }
        if (grep(/map_case_components/, @$fields)) {
            push @supptables, "INNER JOIN test_case_components AS tccomps " .
                              "ON test_cases.case_id = tccomps.case_id";
            push @supptables, "INNER JOIN components AS map_case_components " .
                              "ON tccomps.component_id = map_case_components.id";
        }
        if (grep(/map_case_product/, @$fields)) {
            push(@supptables, "INNER JOIN test_case_plans AS map_case_plans " .
                              "ON test_cases.case_id = map_case_plans.case_id");
            push(@supptables, "INNER JOIN test_plans AS map_product_plans " .
                              "ON map_case_plans.plan_id = map_product_plans.plan_id");
            push(@supptables, "INNER JOIN products AS map_case_product " .
                              "ON map_product_plans.product_id = map_case_product.id");
        }
        if (grep(/map_case_tags/, @$fields)) {
            push @supptables, "INNER JOIN test_case_tags AS tctags " .
                              "ON test_cases.case_id = tctags.case_id";
            push @supptables, "INNER JOIN test_tags AS map_case_tags " .
                              "ON tctags.tag_id = map_case_tags.tag_id";
        }
        if (grep(/map_case_author/, @$fields)) {
            push @supptables, "INNER JOIN profiles AS map_case_author " .
                              "ON test_cases.author_id = map_case_author.userid";
        }
        if (grep(/map_default_tester/, @$fields)) {
            push @supptables, "INNER JOIN profiles AS map_default_tester " .
                              "ON test_cases.default_tester_id = map_default_tester.userid";
        }
        ## Runs ##
            
        if (grep(/map_run_product/, @$fields)) {
            push @supptables, "INNER JOIN test_plans " .
                              "ON test_runs.plan_id = test_plans.plan_id";
            push @supptables, "INNER JOIN products AS map_run_product " .
                              "ON test_plans.product_id = map_run_product.id";
        }
        if (grep(/map_run_build/, @$fields)) {
            push @supptables, "INNER JOIN test_builds AS map_run_build " .
                              "ON test_runs.build_id = map_run_build.build_id";
        }
        if (grep(/map_run_milestone/, @$fields)) {
            push @supptables, "INNER JOIN test_builds AS map_run_milestone " .
                              "ON test_runs.build_id = map_run_milestone.build_id";
        }
        if (grep(/map_run_environment/, @$fields)) {
            push @supptables, "INNER JOIN test_environments AS map_run_environment " .
                              "ON test_runs.environment_id = map_run_environment.environment_id";
        }
        if (grep(/map_run_tags/, @$fields)) {
            push @supptables, "INNER JOIN test_run_tags " .
                              "ON test_runs.run_id = test_run_tags.run_id";
            push @supptables, "INNER JOIN test_tags AS map_run_tags " .
                              "ON test_run_tags.tag_id = map_run_tags.tag_id";
        }
        if (grep(/map_run_manager/, @$fields)) {
            push @supptables, "INNER JOIN profiles AS map_run_manager " .
                              "ON test_runs.manager_id = map_run_manager.userid";
        }

        ## Plans ##
        if (grep(/map_plan_type/, @$fields)) {
            push @supptables, "INNER JOIN test_plan_types AS map_plan_type " .
                              "ON test_plans.type_id = map_plan_type.type_id";
        }
        if (grep(/map_plan_product/, @$fields)) {
            push @supptables, "INNER JOIN products AS map_plan_product " .
                              "ON test_plans.product_id = map_plan_product.id";
        }
        if (grep(/map_plan_tags/, @$fields)) {
            push @supptables, "INNER JOIN test_plan_tags " .
                              "ON test_plans.plan_id = test_plan_tags.plan_id";
            push @supptables, "INNER JOIN test_tags AS map_plan_tags " .
                              "ON test_plan_tags.tag_id = map_plan_tags.tag_id";
        }
        if (grep(/map_plan_author/, @$fields)) {
            push @supptables, "INNER JOIN profiles AS map_plan_author " .
                              "ON test_plans.author_id = map_plan_author.userid";
        }
        ## Case-runs ##
        if (grep(/map_caserun_assignee/, @$fields)) {
            push @supptables, "INNER JOIN profiles AS map_caserun_assignee " .
                              "ON test_case_runs.assignee = map_caserun_assignee.userid";
        }
        if (grep(/map_caserun_testedby/, @$fields)) {
            push @supptables, "INNER JOIN profiles AS map_caserun_testedby " .
                              "ON test_case_runs.testedby = map_caserun_testedby.userid";
        }
        if (grep(/map_caserun_build/, @$fields)) {
            push @supptables, "INNER JOIN test_builds AS map_caserun_build " .
                              "ON test_case_runs.build_id = map_caserun_build.build_id";
        }
        if (grep(/map_caserun_environment/, @$fields)) {
            push @supptables, "INNER JOIN test_environments AS map_caserun_environment " .
                              "ON test_case_runs.environment_id = map_caserun_environment.environment_id";
        }
        if (grep(/map_caserun_status/, @$fields)) {
            push @supptables, "INNER JOIN test_case_run_status AS map_caserun_status " .
                              "ON test_case_runs.case_run_status_id = map_caserun_status.case_run_status_id";
        }
        if (grep(/map_caserun_milestone/, @$fields)) {
            push @supptables, "INNER JOIN test_builds AS map_caserun_milestone " .
                              "ON test_case_runs.build_id = map_caserun_milestone.build_id";
        }
        if (grep(/map_caserun_case_tags/, @$fields)) {
            push @supptables, "INNER JOIN test_case_tags AS tctags " .
                              "ON test_case_runs.case_id = tctags.case_id";
            push @supptables, "INNER JOIN test_tags AS map_caserun_case_tags " .
                              "ON tctags.tag_id = map_caserun_case_tags.tag_id";
        }
        if (grep(/map_caserun_run_tags/, @$fields)) {
            push @supptables, "INNER JOIN test_run_tags " .
                              "ON test_case_runs.run_id = test_run_tags.run_id";
            push @supptables, "INNER JOIN test_tags AS map_run_tags " .
                              "ON test_run_tags.tag_id = map_caserun_run_tags.tag_id";
        }
        if (grep(/map_caserun_cases/, @$fields)) {
            push @supptables, "INNER JOIN test_cases AS map_caserun_cases " .
                              "ON test_case_runs.case_id = map_caserun_cases.case_id";
        }
        if (grep(/map_caserun_priority/, @$fields)) {
            push @supptables, "INNER JOIN test_cases AS map_caserun_cases " .
                              "ON test_case_runs.case_id = map_caserun_cases.case_id";
            push @supptables, "INNER JOIN priority AS map_caserun_priority " .
                              "ON map_caserun_cases.priority_id = map_caserun_priority.case_id";
        }
        if (grep(/map_caserun_default_tester/, @$fields)) {
            push @supptables, "INNER JOIN test_cases AS map_caserun_cases " .
                              "ON test_case_runs.case_id = map_caserun_cases.case_id";
            push @supptables, "INNER JOIN profiles AS map_caserun_default_tester " .
                              "ON map_caserun_cases.default_tester_id = map_caserun_default_tester.userid";
        }
        if (grep(/map_caserun_category/, @$fields)) {
            push @supptables, "INNER JOIN test_cases AS map_caserun_cases " .
                              "ON test_case_runs.case_id = map_caserun_cases.case_id";
            push @supptables, "INNER JOIN test_case_categories AS map_caserun_category " .
                              "ON map_caserun_cases.category_id = map_caserun_category.category_id";
        }
        if (grep(/map_caserun_components/, @$fields)) {
            push @supptables, "INNER JOIN test_cases AS map_caserun_cases " .
                              "ON test_case_runs.case_id = map_caserun_cases.case_id";
            push @supptables, "INNER JOIN test_case_components AS case_components " .
                              "ON map_caserun_cases.case_id = case_components.case_id";
            push @supptables, "INNER JOIN components AS map_caserun_components " .
                              "ON case_components.component_id = map_caserun_components.id";
        }
        
    }
    # Set up tables for access control
    unless (Bugzilla->user->in_group('Testers')){
        if ($obj eq 'case'){
           push(@supptables, "INNER JOIN test_case_plans AS case_plans " .
                  "ON test_cases.case_id = case_plans.case_id");
           push(@supptables, "INNER JOIN test_plans " .
                  "ON case_plans.plan_id = test_plans.plan_id");    }
        elsif ($obj eq 'case_run'){
           push(@supptables, "INNER JOIN test_case_runs AS case_runs " .
                  "ON test_runs.run_id = case_runs.run_id");
           push(@supptables, "INNER JOIN test_cases " .
                  "ON case_runs.case_id = test_cases.case_id");
        }
        elsif ($obj eq 'run'){
           push(@supptables,  "INNER JOIN test_plans " .
                  "ON test_runs.plan_id = test_plans.plan_id");
        }
    
        push @supptables, "INNER JOIN test_plan_permissions ON test_plans.plan_id = test_plan_permissions.plan_id";
        push @supptables, "INNER JOIN user_group_map AS map_testers ON map_testers.user_id = test_plan_permissions.userid";
        push @supptables, "INNER JOIN groups on map_testers.group_id = groups.id";
        push @wherepart, "((test_plan_permissions.permissions > 0 AND test_plan_permissions.userid = ". Bugzilla->user->id 
                          .") OR (map_testers.user_id = ". Bugzilla->user->id ." AND groups.name = 'Testers'))";
    }    
    # Set up tables for field sort order
    my $order = $cgi->param('order') || '';
    if ($order eq 'author') {        
        push @supptables, "INNER JOIN profiles as map_author ON map_author.userid = test_". $obj ."s.author_id";
        push @orderby, 'map_author.login_name';
    }
    elsif ($order eq 'manager') {        
        push @supptables, "INNER JOIN profiles as map_manager ON map_manager.userid = test_". $obj ."s.manager_id";
        push @orderby, 'map_manager.login_name';
    }
    elsif ($order eq 'assignee') {        
        push @supptables, "LEFT JOIN profiles as map_assignee ON map_assignee.userid = test_". $obj ."s.assignee";
        push @orderby, 'map_assignee.login_name';
    }
    elsif ($order eq 'testedby') {        
        push @supptables, "LEFT JOIN profiles as map_testedby ON map_testedby.userid = test_". $obj ."s.testedby";
        push @orderby, 'map_testedby.login_name';
    }
    elsif ($order eq 'tester') {        
        push @supptables, "LEFT JOIN profiles as map_tester ON map_tester.userid = test_". $obj ."s.default_tester_id";
        push @orderby, 'map_tester.login_name';
    }
    elsif ($order eq 'product') {        
        push @supptables, "LEFT JOIN products ON products.id = test_". $obj ."s.product_id";
        push @orderby, 'products.name';
    }
    elsif ($order eq 'build') {        
        push @supptables, "INNER JOIN test_builds AS build ON build.build_id = test_". $obj ."s.build_id";
        push @orderby, 'build.name';
    }
    elsif ($order eq 'environment') {        
        push @supptables, "INNER JOIN test_environments AS env ON env.environment_id = test_". $obj ."s.environment_id";
        push @orderby, 'env.name';
    }
    elsif ($order eq 'plan_type') {        
        push @supptables, "INNER JOIN test_plan_types AS ptype ON ptype.type_id = test_plans.type_id";
        push @orderby, 'ptype.name';
    }
    elsif ($order eq 'plan_prodver') {        
        push @supptables, "INNER JOIN versions ON versions.value = test_plans.default_product_version";
        push @orderby, 'versions.value';
    }
    elsif ($order eq 'priority') {
        if ($obj eq 'case_run'){
            push @supptables, "INNER JOIN test_cases ON test_cases.case_id = test_case_runs.case_id";
        }
        push @supptables, "INNER JOIN priority ON priority.id = test_cases.priority_id";
        push @orderby, 'test_cases.priority_id';
    }
    elsif ($order eq 'build') {
        push @supptables, "INNER JOIN test_builds ON test_builds.build_id = test_case_runs.build_id";
        push @orderby, 'test_builds.name';
    }
    elsif ($order eq 'case_run_status') {
        push @supptables, "INNER JOIN test_case_run_status as case_run_status ON case_run_status.case_run_status_id = test_case_runs.case_run_status_id";
        push @orderby, 'case_run_status.sortkey';
    }
    elsif ($order eq 'category') {
        if ($obj eq 'case_run'){
            push @supptables, "INNER JOIN test_cases ON test_cases.case_id = test_case_runs.case_id";
        }
        push @supptables, "INNER JOIN test_case_categories AS categories ON test_cases.category_id = categories.category_id";
        push @orderby, 'categories.name';
    }
    elsif ($order eq 'case_status') {
        push @supptables, "INNER JOIN test_case_status AS case_status ON test_cases.case_status_id = case_status.case_status_id";
        push @orderby, 'case_status.name';
    }
    elsif ($order eq 'summary') {
        if ($obj eq 'case_run'){
            push @supptables, "INNER JOIN test_cases AS cases ON cases.case_id = test_case_runs.case_id";
            push @orderby, 'cases.summary';
        }
        else{            
            push @orderby, 'test_'. $obj .'s.summary';
        }
    }
    elsif ($order eq 'created') {
        push @orderby, 'test_'. $obj .'s.creation_date';
    }
    elsif ($order eq 'name') {
        push @orderby, 'test_'. $obj .'s.name';
    }
    else{
        if ($order){
            trick_taint($order) if ($order);
            push @orderby, 'test_'. $obj .'s.' . $order;
        }
    }
    
    my @funcdefs =
    (
         "^category," => sub {
               if ($obj eq 'case_run'){
                   push(@supptables,
                          "INNER JOIN test_cases " .
                          "ON test_case_runs.case_id = test_cases.case_id");
               }                   
               push(@supptables,
                      "INNER JOIN test_case_categories AS categories " .
                      "ON test_cases.category_id = categories.category_id");
               $f = "categories.name";
         },
         "^category_id," => sub {
               if ($obj eq 'case_run'){
                   push(@supptables,
                          "INNER JOIN test_cases " .
                          "ON test_case_runs.case_id = test_cases.case_id");
               }                   
               
               $f = "test_cases.category_id";
         },
         "^build," => sub {
               my $str = '';
               $str = 'case_' if ($obj eq 'case_run');
               push(@supptables,
                      "INNER JOIN test_builds AS builds " .
                      "ON test_". $str ."runs.build_id = builds.build_id");
               $f = "builds.name";
         },
         "^(tcaction|tceffect)," => sub {
               push(@supptables,
                      "LEFT JOIN test_case_texts AS case_texts " .
                      "ON test_cases.case_id = case_texts.case_id");
               my $fid = ($1 eq 'tcaction' ? 'action' : 'effect');
               $f = "case_texts.$fid";
         },
         "^plan_text," => sub {
               push(@supptables,
                      "LEFT JOIN test_plan_texts AS plan_texts " .
                      "ON test_plans.plan_id = plan_texts.plan_id");
               $f = "plan_texts.plan_text";
         },
         "^prod_name," => sub {
               push(@supptables,
                    "INNER JOIN products ".
                    "ON test_". $obj ."s.product_id = products.id");
               $f = 'products.name';
         },
         "^case_status," => sub {
               push(@supptables,
                    "INNER JOIN test_case_status AS case_status " . 
                    "ON test_cases.case_status_id = case_status.case_status_id");
               $f = 'case_status.name';      
         },
         "^priority," => sub {
               push(@supptables,
                    "INNER JOIN priority ".
                    "ON test_". $obj ."s.priority_id = priority.id");
               $f = 'priority.value';      
         },
         "^environment," => sub {
               push(@supptables,
                    "INNER JOIN test_environments ".
                    "ON test_". $obj ."s.environment_id = test_environments.environment_id");
               $f = 'test_environments.name';      
         },
         "^plan_type," => sub {
               push(@supptables,
                    "INNER JOIN test_plan_types ".
                    "ON test_plans.type_id = test_plan_types.type_id");
               $f = 'test_plan_types.name';      
         },
         "^case_run_status," => sub {
               push(@supptables,
                    "INNER JOIN test_case_run_status AS tcrs ".
                    "ON test_case_runs.case_run_status_id = tcrs.case_run_status_id");
               $f = 'tcrs.name';      
         },
         "^env_products," => sub {
               push(@supptables,
                    "INNER JOIN products as env_products
                     ON test_environments.product_id = env_products.id");
               $f = 'env_products.id'      
         },
         "^env_categories," => sub {
               push(@supptables,
                    "INNER JOIN test_environment_map
                     ON test_environments.environment_id = test_environment_map.environment_id");
               push(@supptables,
                    "INNER JOIN test_environment_element
                     ON test_environment_map.element_id = test_environment_element.element_id");
               push(@supptables,
                    "INNER JOIN test_environment_category
                     ON test_environment_element.env_category_id = test_environment_category.env_category_id");
               $f = 'test_environment_category.env_category_id'      
         },
         "^env_elements," => sub {
               push(@supptables,
                    "INNER JOIN test_environment_map as env_map_elements
                     ON test_environments.environment_id = env_map_elements.environment_id");
               push(@supptables,
                    "INNER JOIN test_environment_element as env_element
                     ON env_map_elements.element_id = env_element.element_id");
               $f = 'env_element.element_id'      
         },
         "^env_properties," => sub {
               push(@supptables,
                    "INNER JOIN test_environment_map as env_map_properties
                     ON test_environments.environment_id = env_map_properties.environment_id");
               push(@supptables,
                    "INNER JOIN test_environment_property as env_property
                     ON env_map_properties.property_id = env_property.property_id");
               $f = 'env_property.property_id'      
         },
         "^env_expressions," => sub {
               push(@supptables,
                    "INNER JOIN test_environment_map as env_map_value
                     ON test_environments.environment_id = env_map_value.environment_id");
               $f = 'env_map_value.value_selected'      
         },
         "^env_value_selected," => sub {
               push(@supptables,
                    "INNER JOIN test_environment_map as env_map_value_selected
                     ON test_environments.environment_id = env_map_value_selected.environment_id");
               $f = 'env_map_value_selected.value_selected'      
         },
         "^component," => sub {
               if ($obj eq 'case_run'){
                    push(@supptables,
                        "INNER JOIN test_cases 
                         ON test_cases.case_id = test_case_runs.case_id");
               }
               push(@supptables,
                      "INNER JOIN test_case_components AS tc_components " .
                      "ON test_cases.case_id = tc_components.case_id");
               push(@supptables,
                      "INNER JOIN components ".
                      "ON components.id = tc_components.component_id");
               $f = "components.name";
         },
         "^priority_id," => sub {
               if ($obj eq 'case_run'){
                    push(@supptables,
                        "INNER JOIN test_cases 
                         ON test_cases.case_id = test_case_runs.case_id");
               }
               $f = "test_cases.priority_id";
         },
         "^isautomated," => sub {
               if ($obj eq 'case_run'){
                    push(@supptables,
                        "INNER JOIN test_cases 
                         ON test_cases.case_id = test_case_runs.case_id");
               }
               $f = "test_cases.isautomated";
         },
         "^milestone," => sub {
               push(@supptables,
                      "INNER JOIN test_builds AS builds " .
                      "ON test_runs.build_id = builds.build_id");
               push(@supptables,
                      "INNER JOIN milestones ".
                      "ON milestones.value = builds.milestone");
               $f = "milestones.value";
         },
         "^bug," => sub {
             if ($obj eq 'case_run'){
               push(@supptables,
                      "INNER JOIN test_case_bugs AS case_bugs " .
                      "ON test_case_runs.case_run_id = case_bugs.case_run_id");
               push(@supptables,
                      "INNER JOIN bugs ".
                      "ON case_bugs.bug_id = bugs.bug_id");
             }
             elsif($obj eq 'case'){
               push(@supptables,
                      "INNER JOIN test_case_bugs AS case_bugs " .
                      "ON test_cases.case_id = case_bugs.case_id");
               push(@supptables,
                      "INNER JOIN bugs ".
                      "ON case_bugs.bug_id = bugs.bug_id");
             }
               $f = "bugs.bug_id";
         },
         "^case_summary," => sub {
               push(@supptables,
                      "INNER JOIN test_cases AS cases " .
                      "ON cases.case_id = test_case_runs.case_id");
               $f = "cases.summary";
         },
         
         "^tags," => sub {
               if ($obj eq 'case_run'){
                   push(@supptables,
                      "INNER JOIN test_cases " .
                      "ON test_case_runs.case_id = test_cases.case_id");
                   push(@supptables,
                      "INNER JOIN test_case_tags  AS case_tags " .
                      "ON test_cases.case_id = case_tags.case_id");
                   push(@supptables,
                       "INNER JOIN test_tags " .
                       "ON case_tags.tag_id = test_tags.tag_id");
               }
               else{
                   push(@supptables,
                          "INNER JOIN test_". $obj ."_tags  AS ". $obj ."_tags " .
                          "ON test_". $obj ."s.". $obj ."_id = ". $obj ."_tags.". $obj ."_id");
                   push(@supptables,
                          "INNER JOIN test_tags " .
                          "ON ". $obj ."_tags.tag_id = test_tags.tag_id");
                   }
               $f = "test_tags.tag_name";
         },
         "^requirement," => sub {
             if ($obj eq 'case_run'){
                   push(@supptables,
                      "INNER JOIN test_cases " .
                      "ON test_case_runs.case_id = test_cases.case_id");
             }
             $f = "test_cases.requirement"; 
         },
         "^case_plan_id," => sub {
               push(@supptables,
                      "INNER JOIN test_case_plans AS case_plans " .
                      "ON test_cases.case_id = case_plans.case_id");
               push(@supptables,
                      "INNER JOIN test_plans " .
                      "ON case_plans.plan_id = test_plans.plan_id");
               $f = "test_plans.plan_id";
         },
         "^plan_case_id," => sub {
               push(@supptables,
                      "INNER JOIN test_case_plans AS case_plans " .
                      "ON test_plans.plan_id = case_plans.plan_id");
               push(@supptables,
                      "INNER JOIN test_cases " .
                      "ON case_plans.case_id = test_cases.case_id");
               $f = "test_cases.case_id";
         },
         "^cases_in_runs," => sub {
               push(@supptables,
                      "INNER JOIN test_case_runs AS case_runs " .
                      "ON test_cases.case_id = case_runs.case_id");
               push(@supptables,
                      "INNER JOIN test_runs " .
                      "ON case_runs.run_id = test_runs.run_id");
               $f = "test_runs.run_id";
         },
         "^run_plan_id," => sub {
               $f = "test_runs.plan_id";
         },
         "^run_case_id," => sub {
               push(@supptables,
                      "INNER JOIN test_case_runs AS case_runs " .
                      "ON test_runs.run_id = case_runs.run_id");
               push(@supptables,
                      "INNER JOIN test_cases " .
                      "ON case_runs.case_id = test_cases.case_id");
               $f = "test_cases.case_id";
         },
         "^caserun_plan_id," => sub {
               push(@supptables,
                      "INNER JOIN test_runs " .
                      "ON test_case_runs.run_id = test_runs.run_id");
               push(@supptables,
                      "INNER JOIN test_plans " .
                      "ON test_runs.plan_id = test_plans.plan_id");
               $f = "test_plans.plan_id";
         },
         "^case_prod," => sub {
               push(@supptables,
                      "INNER JOIN test_case_plans AS case_plans " .
                      "ON test_cases.case_id = case_plans.case_id");
               push(@supptables,
                      "INNER JOIN test_plans " .
                      "ON case_plans.plan_id = test_plans.plan_id");
               push(@supptables,
                      "INNER JOIN products " .
                      "ON test_plans.product_id = products.id");
               if ($cgi->param('product_id')){
                   $f = "test_plans.product_id"; 
               }
               else {
                   $f = "products.name";
               }

         },
         "^caserun_prod," => sub {
               push(@supptables,
                      "INNER JOIN test_runs " .
                      "ON test_case_runs.run_id = test_runs.run_id");
               push(@supptables,
                      "INNER JOIN test_plans " .
                      "ON test_runs.plan_id = test_plans.plan_id");
               push(@supptables,
                      "INNER JOIN products " .
                      "ON test_plans.product_id = products.id");
               if ($cgi->param('product_id')){
                   $f = "test_plans.product_id"; 
               }
               else {
                   $f = "products.name";
               }

         },
         "^run_prod," => sub {
               push(@supptables,
                      "INNER JOIN test_plans " .
                      "ON test_runs.plan_id = test_plans.plan_id");
               push(@supptables,
                      "INNER JOIN products " .
                      "ON test_plans.product_id = products.id");
               if ($cgi->param('product_id')){
                   $f = "test_plans.product_id"; 
               }
               else {
                   $f = "products.name";
               }
         },
         "^stop_date," => sub {
            if ($obj eq 'case_run'){
                push(@supptables,
                      "INNER JOIN test_runs " .
                      "ON test_case_runs.run_id = test_runs.run_id");
                $f = "test_runs.stop_date";
            }
         },
         "^run_product_version," => sub {
               push(@supptables,
                      "INNER JOIN test_runs " .
                      "ON test_case_runs.run_id = test_runs.run_id");
               $f = "test_runs.product_version";
         },
         "^(author|manager|default_tester)," => sub {
               push(@supptables,
                      "INNER JOIN profiles AS map_$1 " . 
                      "ON test_". $obj ."s.". $1 ."_id = map_$1.userid");
               $f = "map_$1.login_name"; 
         },
         "^(assignee|testedby)," => sub {
               if ($obj eq 'run'){
                   push(@supptables,
                      "LEFT JOIN test_case_runs AS case_run " . 
                      "ON case_run.run_id = test_runs.run_id");
                   push(@supptables,
                      "INNER JOIN profiles AS map_$1 " . 
                      "ON case_run.". $1 ." = map_$1.userid");
               }
               else {
                   push(@supptables,
                      "LEFT JOIN profiles AS map_$1 " . 
                      "ON test_". $obj ."s.". $1 ." = map_$1.userid");
               }
               $f = "map_$1.login_name";
               
         },
         ",isnotnull" => sub {
             $term = "$ff is not null";
         },
         ",isnull" => sub {
             $term = "$ff is null";
         },
         ",equals" => sub {
             $term = "$ff = $q";
         },
         ",notequals" => sub {
             $term = "$ff != $q";
         },
         ",casesubstring" => sub {
             $term = $dbh->sql_position($q, $ff) . " > 0";
         },
         ",substring" => sub {
             $term = $dbh->sql_position(lc($q), "LOWER($ff)") . " > 0";
         },
         ",substr" => sub {
             $funcsbykey{",substring"}->();
         },
         ",notsubstring" => sub {
             $term = $dbh->sql_position(lc($q), "LOWER($ff)") . " = 0";
         },
         ",regexp" => sub {
             $term = "$ff " . $dbh->sql_regexp() . " $q";
         },
         ",notregexp" => sub {
             $term = "$ff " . $dbh->sql_not_regexp() . " $q";
         },
         ",lessthan" => sub {
             $term = "$ff < $q";
         },
         ",greaterthan" => sub {
             $term = "$ff > $q";
         },
         ",anyexact" => sub {
             my @list;
             foreach my $w (split(/,/, $v)) {
                 push(@list, $dbh->quote($w));
             }
             if (@list) {
                 $term = "$ff IN (" . join (',', @list) . ")";
             }
         },
         ",anywordssubstr" => sub {
             $term = join(" OR ", @{GetByWordListSubstr($ff, $v)});
         },
         ",allwordssubstr" => sub {
             $term = join(" AND ", @{GetByWordListSubstr($ff, $v)});
         },
         ",nowordssubstr" => sub {
             my @list = @{GetByWordListSubstr($ff, $v)};
             if (@list) {
                 $term = "NOT (" . join(" OR ", @list) . ")";
             }
         },
         ",anywords" => sub {
             $term = join(" OR ", @{GetByWordList($ff, $v)});
         },
         ",allwords" => sub {
             $term = join(" AND ", @{GetByWordList($ff, $v)});
         },
         ",nowords" => sub {
             my @list = @{GetByWordList($ff, $v)};
             if (@list) {
                 $term = "NOT (" . join(" OR ", @list) . ")";
             }
         },
     );

    if ($cgi->param('case_id')) {
        my $type = "anyexact";
        if ($cgi->param('caseidtype'))
        {
            if ($cgi->param('caseidtype') eq 'exclude') 
            {
                $type = "nowords";
            }
            else
            {
                $type = $cgi->param('caseidtype')
            } 
        }
        if ($obj eq 'run'){
            push(@specialchart, ["run_case_id", $type, join(',', $cgi->param('case_id'))]);
        }
        elsif ($obj eq 'plan'){
            push(@specialchart, ["plan_case_id", $type, join(',', $cgi->param('case_id'))]);
        }
        else{
            push(@specialchart, ["case_id", $type, join(',', $cgi->param('case_id'))]);
        }
    }
    if ($cgi->param('run_id')) {
        my $type = "anyexact";
        if ($cgi->param('runidtype'))
        {
            if ($cgi->param('runidtype') eq 'exclude') 
            {
                $type = "nowords";
            }
            else
            {
                $type = $cgi->param('runidtype')
            } 
        }
        if ($obj eq 'case'){
            push(@specialchart, ["cases_in_runs", $type, join(',', $cgi->param('run_id'))]);
        }
        else {
            push(@specialchart, ["run_id", $type, join(',', $cgi->param('run_id'))]);
        }
    }
    if ($cgi->param('plan_id')) {
        my $type = "anyexact";
        if ($cgi->param('planidtype'))
        {
            if ($cgi->param('planidtype') eq 'exclude') 
            {
                $type = "nowords";
            }
            else
            {
                $type = $cgi->param('planidtype')
            } 
        }
        if ($obj eq 'case'){
            push(@specialchart, ["case_plan_id", $type, join(',', $cgi->param('plan_id'))]);
            if ($cgi->param('exclude')){
                my @exclusions = split(/,/, $cgi->param('exclude'));
                foreach (@exclusions){
                    detaint_natural($_);
                }
                push @wherepart, 'test_cases.case_id NOT IN ('. join(',', @exclusions) .')';
            }
        }
        elsif ($obj eq 'run'){
            push(@specialchart, ["run_plan_id", $type, join(',', $cgi->param('plan_id'))]);
        }
        elsif ($obj eq 'case_run'){
            push(@specialchart, ["caserun_plan_id", $type, join(',', $cgi->param('plan_id'))]);
        }
        else{
            push(@specialchart, ["plan_id", $type, join(',', $cgi->param('plan_id'))]);
        }
    }
    if ($cgi->param('bug_id')) {
        my $type = "anyexact";
        if ($cgi->param('bugidtype') && $cgi->param('bugidtype') eq 'exclude') {
            $type = "nowords";
        }
        push(@specialchart, ["bug", $type, join(',', $cgi->param('bug_id'))]);
    }
    if ($cgi->param("product_id") || $cgi->param("product")){
        my $attribute = $cgi->param("product_id") ? "product_id" : "product";
        my $type = "anyexact";
        if ($cgi->param('prodidtype') && $cgi->param('prodidtype') eq 'exclude') {
            $type = "nowords";
        }
        if ($obj eq 'run'){
            push(@specialchart, ["run_prod", $type, join(',', $cgi->param($attribute))]);
        }
        elsif ($obj eq 'case'){
            push(@specialchart, ["case_prod", $type, join(',', $cgi->param($attribute))]);
        }
        elsif ($obj eq 'case_run'){
            push(@specialchart, ["caserun_prod", $type, join(',', $cgi->param($attribute))]);
        }
        else{
            if ($cgi->param("product")){
                push(@specialchart, ["prod_name", $type, join(',', $cgi->param($attribute))]);
            }
            else{
                push(@specialchart, ["product_id", $type, join(',', $cgi->param($attribute))]);
            }
        }
    }        
    # Check the Multi select fields and add them to the chart
    my @legal_fields = ("case_status_id", "category", "category_id", "priority_id",
                        "component", "isautomated", "case_run_status_id",
                        "default_product_version", "run_product_version", "type_id", 
                        "build", "build_id", "environment_id", "milestone", "env_products",
                        "env_categories", "env_elements", "env_properties", 
                        "env_expressions", "case_status", "priority", "environment",
                        "plan_type", "case_run_status");

    foreach my $field ($cgi->param()) {
        if (lsearch(\@legal_fields, $field) != -1) {
            push(@specialchart, [$field, "anyexact",
                                 join(',', $cgi->param($field))]);
        }
    }
    # 19.01.2007 - Changed multiselct version fields to 'version'.
    # Changing them above in @legal_fields could break API funtionality
    # So we redifine them here.
    if (defined $cgi->param('version')){
        my $field;
        if ($obj eq 'case_run'){
            $field = 'run_product_version';
        }
        elsif ($obj eq 'run'){
            $field = 'product_version';
        }
        push(@specialchart, [$field, "anyexact", join(',', $cgi->param('version'))]);
    }
    if (defined $cgi->param('run_status')){
        my @sta = $cgi->param('run_status');
        unless (scalar @sta > 1){
            if ($cgi->param('run_status') == 1){
                push(@specialchart, ['stop_date', 'isnotnull', 'null']);
            }
            else {
                push(@specialchart, ['stop_date', 'isnull', 'null']);
            }
        }
    }
    if (defined $cgi->param('close_date')){
        my @sta = $cgi->param('close_date');
        unless (scalar @sta > 1){
            if ($cgi->param('close_date') == 1){
                push(@specialchart, ['close_date', 'isnotnull', 'null']);
            }
            else {
                push(@specialchart, ['close_date', 'isnull', 'null']);
            }
        }
    }
    # Check the tags and add them to the chart
    if ($cgi->param('tags')) {
        my $t = $cgi->param('tags_type');
        if (!$t || $t eq "or") {
            $t = "anywords";
        }
        push(@specialchart, ["tags", $t, $cgi->param('tags')]);
    }
    # Check for author
    my @clist;
    foreach my $profile ("author", "manager", "default_tester", 
                         "assignee", "testedby"){
        $t = $cgi->param($profile . "_type") || '';
        if ($t eq "exact"  || $t eq '') {
            $t = "anyexact";
            if ($cgi->param($profile)){
                foreach my $name (split(',', $cgi->param($profile))) {
                    $name = trim($name);
                    if ($name) {
                        login_to_id($name);
                    }
                }
            }
        }
        if ($cgi->param($profile)){
            if ($cgi->param('andor')){
                push(@specialchart, [$profile, $t, trim($cgi->param($profile))]);
            }
            else{
                push(@clist, $profile, $t, trim($cgi->param($profile)));
            }
        }
    }
    if (@clist) {
        push(@specialchart, \@clist);
    }
    
    # check static text fields
    foreach my $f ("case_summary", "summary", "tcaction", "tceffect", "script",
                   "requirement", "name", "plan_text", "environment_name",
                   "notes", "env_value_selected") {
        if (defined $cgi->param($f)) {
            my $s = trim($cgi->param($f));
            if ($s ne "") {
                trick_taint($s);
                my $type = $cgi->param($f . "_type");
                push(@specialchart, [$f, $type, $s]);
            }
        }
    }
    if ($obj eq 'plan'){
        unless ($cgi->param('isactive')){
            push @wherepart, 'test_plans.isactive = 1';
        }
    }
    if ($obj eq 'case_run'){
        unless ($cgi->param('isactive')){
            push @wherepart, 'test_case_runs.iscurrent = 1';
        }
    }

    my @funcnames;
    while (@funcdefs) {
        my $key = shift(@funcdefs);
        my $value = shift(@funcdefs);
        if ($key =~ /^[^,]*$/) {
            die "All defs in %funcs must have a comma in their name: $key";
        }
        if (exists $funcsbykey{$key}) {
            die "Duplicate key in %funcs: $key";
        }
        $funcsbykey{$key} = $value;
        push(@funcnames, $key);
    }

    # first we delete any sign of "Chart #-1" from the HTML form hash
    # since we want to guarantee the user didn't hide something here
    my @badcharts = grep /^(field|type|value)-1-/, $cgi->param();
    foreach my $field (@badcharts) {
        $cgi->delete($field);
    }

    # now we take our special chart and stuff it into the form hash
    my $chart = -1;
    my $row = 0;
    foreach my $ref (@specialchart) {
        my $col = 0;
        while (@$ref) {
            $cgi->param("field$chart-$row-$col", shift(@$ref));
            $cgi->param("type$chart-$row-$col", shift(@$ref));
            $cgi->param("value$chart-$row-$col", shift(@$ref));
            if ($debug) {
                print qq{<p>$cgi->param("field$chart-$row-$col") | $cgi->param("type$chart-$row-$col") | $cgi->param("value$chart-$row-$col")*</p>\n};
            }
            $col++;

        }
        $row++;
    }
    # get a list of field names to verify the user-submitted chart fields against
    my $ref = $dbh->selectall_arrayref("SELECT name, fieldid FROM test_fielddefs");
    foreach my $list (@{$ref}) {
        my ($name, $id) = @{$list};
        $chartfields{$name} = $id;
    }

    $row = 0;
    for ($chart=-1 ;
         $chart < 0 || $cgi->param("field$chart-0-0") ;
         $chart++) {
        $chartid = $chart >= 0 ? $chart : "";
        my @chartandlist = ();
        for ($row = 0 ;
             $cgi->param("field$chart-$row-0") ;
             $row++) {
            my @orlist;
            for (my $col = 0 ;
                 $cgi->param("field$chart-$row-$col") ;
                 $col++) {
                $f = $cgi->param("field$chart-$row-$col") || "noop";
                $t = $cgi->param("type$chart-$row-$col") || "noop";
                $v = $cgi->param("value$chart-$row-$col");
                $v = "" if !defined $v;
                $v = trim($v);
                if ($f eq "noop" || $t eq "noop" || $v eq "") {
                    next;
                }
                # chart -1 is generated by other code above, not from the user-
                # submitted form, so we'll blindly accept any values in chart -1
                if ((!$chartfields{$f}) && ($chart != -1)) {
                    ThrowCodeError("invalid_field_name", {field => $f});
                }

                # This is either from the internal chart (in which case we
                # already know about it), or it was in %chartfields, so it is
                # a valid field name, which means that it's ok.
                trick_taint($f);
                $q = $dbh->quote($v);
                my $rhs = $v;
                $rhs =~ tr/,//;
                my $func;
                $term = undef;
                foreach my $key (@funcnames) {
                    if ("$f,$t,$rhs" =~ m/$key/) {
                        my $ref = $funcsbykey{$key};
                        if ($debug) {
                            print "<p>$key ($f , $t , $rhs ) => ";
                        }
                        $ff = $f;
                       if ($f !~ /\./) {
                            $ff = "test_". $obj ."s.$f";
                        }
                        &$ref;
                        if ($debug) {
                            print "$f , $t , $v , $term</p>";
                        }
                        if ($term) {
                            last;
                        }
                    }
                }
                if ($term) {
                    push(@orlist, $term);
                }
                else {
                    # This field and this type don't work together.
                    ThrowCodeError("field_type_mismatch",
                                   { field => $cgi->param("field$chart-$row-$col"),
                                     type => $cgi->param("type$chart-$row-$col"),
                                   });
                }
            }
            if (@orlist) {
                @orlist = map("($_)", @orlist) if (scalar(@orlist) > 1);
                push(@chartandlist, "(" . join(" OR ", @orlist) . ")");
            }
        }
        if (@chartandlist) {
            if ($cgi->param("negate$chart")) {
                push(@andlist, "NOT(" . join(" AND ", @chartandlist) . ")");
            } else {
                push(@andlist, "(" . join(" AND ", @chartandlist) . ")");
            }
        }
    }

    # The ORDER BY clause goes last, but can require modifications
    # to other parts of the query, so we want to create it before we
    # write the FROM clause.
    foreach my $orderitem (@inputorder) {
        # Some fields have 'AS' aliases. The aliases go in the ORDER BY,
        # not the whole fields.
        # XXX - Ideally, we would get just the aliases in @inputorder,
        # and we'd never have to deal with this.
        if ($orderitem =~ /\s+AS\s+(.+)$/i) {
            $orderitem = $1;
        }
        BuildOrderBy($orderitem, \@orderby);
    }
    # Now JOIN the correct tables in the FROM clause.
    # This is done separately from the above because it's
    # cleaner to do it this way.
    foreach my $orderitem (@inputorder) {
        # Grab the part without ASC or DESC.
        my @splitfield = split(/\s+/, $orderitem);
        if ($specialorderjoin{$splitfield[0]}) {
            push(@supptables, $specialorderjoin{$splitfield[0]});
        }
    }
    if ($debug){
        print "<pre>";
        print join("\n", @supptables);
        print "</pre>";
    }
    my %suppseen = ("test_". $obj ."s" => 1);
    my $suppstring = "test_". $obj ."s";
    my @supplist = (" ");
    foreach my $str (@supptables) {
        if (!$suppseen{$str}) {
            if ($str =~ /^(LEFT|INNER|RIGHT)\s+JOIN/i) {
                $str =~ /^(.*?)\s+ON\s+(.*)$/i;
                my ($leftside, $rightside) = ($1, $2);
                if ($suppseen{$leftside}) {
                    $supplist[$suppseen{$leftside}] .= " AND ($rightside)";
                } else {
                    $suppseen{$leftside} = scalar @supplist;
                    push @supplist, " $leftside ON ($rightside)";
                }
            } else {
                # Do not accept implicit joins using comma operator
                # as they are not DB agnostic
                # ThrowCodeError("comma_operator_deprecated");
                $suppstring .= ", $str";
                $suppseen{$str} = 1;
            }
        }
    }
    $suppstring .= join('', @supplist);
    
    # Make sure we create a legal SQL query.
    @andlist = ("1 = 1") if !@andlist;

    my $query;
    if ($self->{'fields'}){
        $query = "SELECT ". join(",", @{$self->{'fields'}});
    }
    else {
        $query = "SELECT test_". $obj ."s.". $obj. "_id";
    }
    $query .= " FROM $suppstring";
    $query .= " WHERE " . join(' AND ', (@wherepart, @andlist));


    foreach my $field (@fields, @orderby) {
        next if ($field =~ /(AVG|SUM|COUNT|MAX|MIN|VARIANCE)\s*\(/i ||
                 $field =~ /^\d+$/ || $field eq "bugs.bug_id" ||
                 $field =~ /^relevance/);
        if ($field =~ /.*AS\s+(\w+)$/i) {
            push(@groupby, $1) if !grep($_ eq $1, @groupby);
        } else {
            push(@groupby, $field) if !grep($_ eq $field, @groupby);
        }
    }
    $query .= " " . $dbh->sql_group_by("test_". $obj ."s.". $obj ."_id", join(', ', @groupby));


    if (@having) {
        $query .= " HAVING " . join(" AND ", @having);
    }

    if (@orderby) {
        $query .= " ORDER BY " . join(',', @orderby);
    }
    if (defined $page){
        $query .= " LIMIT $pagesize OFFSET ". $page*$pagesize;
    }
    if ($debug) {
        print "<p><code>" . value_quote($query) . "</code></p>\n";
    }
    
    $self->{'sql'} = $query;

}

sub query {
    my $self = shift;
    return $self->{'sql'};    
}

sub GetByWordList {
    my ($field, $strs) = (@_);
    my @list;
    my $dbh = Bugzilla->dbh;

    foreach my $w (split(/[\s,]+/, $strs)) {
        my $word = $w;
        if ($word ne "") {
            $word =~ tr/A-Z/a-z/;
            $word = $dbh->quote(quotemeta($word));
            trick_taint($word);
            $word =~ s/^'//;
            $word =~ s/'$//;
            $word = '(^|[^a-z0-9])' . $word . '($|[^a-z0-9])';
            push(@list, "$field " . $dbh->sql_regexp() . " '$word'");
        }
    }

    return \@list;
}

# Support for "any/all/nowordssubstr" comparison type ("words as substrings")
sub GetByWordListSubstr {
    my ($field, $strs) = (@_);
    my @list;
    my $dbh = Bugzilla->dbh;

    foreach my $word (split(/[\s,]+/, $strs)) {
        trick_taint($word);
        if ($word ne "") {
            push(@list, $dbh->sql_position(lc($dbh->quote($word)),
                                           "LOWER($field)") . " > 0");
        }
    }

    return \@list;
}

=head1 SEE ALSO

Testopia::Table Bugzilla::Search

=head1 AUTHOR

Greg Hendricks <ghendricks@novell.com>

=cut

1;
