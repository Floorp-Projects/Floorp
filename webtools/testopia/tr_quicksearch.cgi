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
# The Original Code is the Bugzilla Testopia System.
#
# The Initial Developer of the Original Code is Greg Hendricks.
# Portions created by Greg Hendricks are Copyright (C) 2006
# Novell. All Rights Reserved.
#
# Contributor(s): Greg Hendricks <ghendricks@novell.com>

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Config;
use Bugzilla::Error;
use Bugzilla::Util;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;

use JSON;

use vars qw($vars);

Bugzilla->login(LOGIN_REQUIRED);

my $template = Bugzilla->template;
my $cgi = Bugzilla->cgi;

my $action = $cgi->param('action') || '';
my $term = trim($cgi->param('query')) || '';

push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

# Quicksearch allows a user to look up any of the major objects in Testopia 
# using a simple prefix. For instance, e:linux will search for all environments
# with linux in the name or tr 33 will bring up Test Run 33.
# If only one is returned, we jump to the appropriate page, otherwise 
# we display the list.

# If we have a term we are using this quicksearch 
if ($term){
    SWITCH: for ($term){
        /^(tag)?[\s:-]+(.*)$/i && do{
            my $text = trim($2);
            print "Location: " . Param('urlbase') . "tr_tags.cgi?tag=" . $text . "\n\n";
            last SWITCH;
        };
        /^(plan|TP|p)?[\s:-]+(.*)$/i && do{
            my $text = trim($2);
            if ($text =~ /^\d+$/){
                print "Location: " . Param('urlbase') . "tr_show_plan.cgi?plan_id=" . $text . "\n\n";
            }
            else{
                $cgi->param('current_tab', 'plan');
                $cgi->param('name_type', 'anywordssubstr');
                $cgi->param('name', $text);
                
                my $search = Bugzilla::Testopia::Search->new($cgi);
                my $table = Bugzilla::Testopia::Table->new('plan', 'tr_list_plans.cgi', $cgi, undef, $search->query);
                if ($table->list_count == 1){
                    print "Location: " . Param('urlbase') . "tr_show_plan.cgi?plan_id=" . ${$table->list}[0]->id . "\n\n";
                }
                else{
                    print "Location: " . Param('urlbase') . "tr_list_plans.cgi?" . $table->get_query_part . "\n\n";
                }
                
            }
            last SWITCH;
        };
        /^(run|TR|r)?[\s:-]+(.*)$/i && do{
            my $text = trim($2);
            if ($text =~ /^\d+$/){
                print "Location: " . Param('urlbase') . "tr_show_run.cgi?run_id=" . $text . "\n\n";
            }
            else{
                $cgi->param('current_tab', 'run');
                $cgi->param('summary_type', 'anywordssubstr');
                $cgi->param('summary', $text);
                
                my $search = Bugzilla::Testopia::Search->new($cgi);
                my $table = Bugzilla::Testopia::Table->new('run', 'tr_list_runs.cgi', $cgi, undef, $search->query);
                if ($table->list_count == 1){
                    print "Location: " . Param('urlbase') . "tr_show_run.cgi?run_id=" . ${$table->list}[0]->id . "\n\n";
                }
                else{
                    print "Location: " . Param('urlbase') . "tr_list_runs.cgi?" . $table->get_query_part . "\n\n";
                }
                
            }
            last SWITCH;
        };
        /^(environment|TE|e|env)?[\s:-]+(.*)$/i && do{
            my $text = trim($2);
            if ($text =~ /^\d+$/){
                print "Location: " . Param('urlbase') . "tr_show_environment.cgi?env_id=" . $text . "\n\n";
            }
            else{
                $cgi->param('current_tab', 'environment');
                $cgi->param('name_type', 'anywordssubstr');
                $cgi->param('name', $text);
                
                my $search = Bugzilla::Testopia::Search->new($cgi);
                my $table = Bugzilla::Testopia::Table->new('environment', 'tr_list_runs.cgi', $cgi, undef, $search->query);
                if ($table->list_count == 1){
                    print "Location: " . Param('urlbase') . "tr_show_environment.cgi?env_id=" . ${$table->list}[0]->id . "\n\n";
                }
                else{
                    print "Location: " . Param('urlbase') . "tr_list_environments.cgi?" . $table->get_query_part . "\n\n";
                }
                
            }
            last SWITCH;
        };
        /^(caserun|TCR|cr)?[\s:-]+(.*)$/i && do{
            my $text = trim($2);
            if ($text =~ /^\d+$/){
                print "Location: " . Param('urlbase') . "tr_show_caserun.cgi?caserun_id=" . $text . "\n\n";
            }
            else{
                $cgi->param('current_tab', 'case_run');
                $cgi->param('notes_type', 'anywordssubstr');
                $cgi->param('notes', $text);
                
                my $search = Bugzilla::Testopia::Search->new($cgi);
                my $table = Bugzilla::Testopia::Table->new('run', 'tr_list_runs.cgi', $cgi, undef, $search->query);
                if ($table->list_count == 1){
                    print "Location: " . Param('urlbase') . "tr_show_caserun.cgi?caserun_id=" . ${$table->list}[0]->id . "\n\n";
                }
                else{
                    print "Location: " . Param('urlbase') . "tr_list_caseruns.cgi?" . $table->get_query_part . "\n\n";
                }
                
            }
            last SWITCH;
        };
        do{
            $term =~ s/^(case|TC|c)?[\s:-]+(.*)$/$2/gi;
            if ($term =~ /^\d+$/){
                print "Location: " . Param('urlbase') . "tr_show_case.cgi?case_id=" . $term . "\n\n";
            }
            else{
                $cgi->param('current_tab', 'case');
                $cgi->param('summary_type', 'anywordssubstr');
                $cgi->param('summary', $term);
                
                my $search = Bugzilla::Testopia::Search->new($cgi);
                my $table = Bugzilla::Testopia::Table->new('case', 'tr_list_cases.cgi', $cgi, undef, $search->query);
                if ($table->list_count == 1){
                    print "Location: " . Param('urlbase') . "tr_show_case.cgi?case_id=" . ${$table->list}[0]->id . "\n\n";
                }
                else{
                    print "Location: " . Param('urlbase') . "tr_list_cases.cgi?" . $table->get_query_part . "\n\n";
                }
                
            }
        };
    }
    
}
############
### Ajax ###
############

# This is where we lookup items typed into Dojo combo boxes
else{
    print $cgi->header;

# Environment Lookup
    if ($action eq 'getenv'){
        my $search = $cgi->param('search');
        my $product_id = $cgi->param('product_id');
        
        detaint_natural($product_id);
        trick_taint($search);
        $search = "%$search%";
        my $dbh = Bugzilla->dbh;
        
        # The order of name and environment are important in the select statment.
        # JSON will convert this to an array of arrays which Dojo will interpret
        # as a select list in the ComboBox widget.
        my $ref;
        if ($product_id){
            $ref = $dbh->selectall_arrayref(
                "SELECT name, environment_id 
                   FROM test_environments 
                  WHERE name like ? AND product_id = ?
                  ORDER BY name",
                  undef, ($search, $product_id));
        }
        else{
            $ref = $dbh->selectall_arrayref(
                "SELECT name, environment_id 
                   FROM test_environments 
                  WHERE name like ?
                  ORDER BY name
                  LIMIT 20",
                  undef, ($search));
        }
        print objToJson($ref);  
    }

# Tag lookup
    elsif ($action eq 'gettag'){
        my $search = $cgi->param('search');
        my @product_ids;
        foreach my $id (split(",", $cgi->param('product_id'))){
            push @product_ids, $id if detaint_natural($id);
        }
        my $product_ids = join(",". @product_ids);
        
        trick_taint($search);
        $search = "%$search%";
        my $dbh = Bugzilla->dbh;
        my $ref;
        my $run_id = $cgi->param('run_id');
        if ($product_ids){
            $ref = $dbh->selectall_arrayref(
                "SELECT tag_name, test_tags.tag_id 
                     FROM test_tags
                    INNER JOIN test_case_tags ON test_tags.tag_id = test_case_tags.tag_id
                    INNER JOIN test_cases on test_cases.case_id = test_case_tags.case_id
                    INNER JOIN test_case_plans on test_case_plans.case_id = test_cases.case_id
                    INNER JOIN test_plans ON test_plans.plan_id = test_case_plans.plan_id
                    WHERE tag_name like ? AND test_plans.product_id IN ($product_ids)  
                 UNION SELECT tag_name, test_tags.tag_id
                     FROM test_tags
                    INNER JOIN test_plan_tags ON test_plan_tags.tag_id = test_tags.tag_id
                    INNER JOIN test_plans ON test_plan_tags.plan_id = test_plans.plan_id
                    WHERE tag_name like ? AND test_plans.product_id IN ($product_ids)
                 UNION SELECT tag_name, test_tags.tag_id
                     FROM test_tags
                    INNER JOIN test_run_tags ON test_run_tags.tag_id = test_tags.tag_id
                    INNER JOIN test_runs ON test_runs.run_id = test_run_tags.run_id
                    INNER JOIN test_plans ON test_plans.plan_id = test_runs.plan_id
                    WHERE tag_name like ? AND test_plans.product_id IN ($product_ids)
                 ORDER BY tag_name",
                  undef, ($search,$search,$search));
        }
        else {
            $ref = $dbh->selectall_arrayref(
                "SELECT tag_name, tag_id 
                   FROM test_tags 
                  WHERE tag_name like ?
                  ORDER BY tag_name
                  LIMIT 20",
                  undef, $search);
        }
        print objToJson($ref);  
    }

# If neither is true above, display the quicksearch form and explaination.
    else{
        $template->process("testopia/quicksearch.html.tmpl", $vars) ||
            ThrowTemplateError($template->error());
    }
}