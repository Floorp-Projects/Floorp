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
# The Original Code is the Bugzilla Test Runner System.
#
# The Initial Developer of the Original Code is Maciej Maczynski.
# Portions created by Maciej Maczynski are Copyright (C) 2001
# Maciej Maczynski. All Rights Reserved.
#
# Contributor(s): Greg Hendricks <ghendricks@novell.com>

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Util;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Config;
use Bugzilla::Testopia::TestPlan;
use Bugzilla::Testopia::Category;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;
use Bugzilla::Testopia::Util;

Bugzilla->login(LOGIN_REQUIRED);

my $cgi = Bugzilla->cgi;

print $cgi->header;
use vars qw($vars $template);
my $template = Bugzilla->template;

push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

my $action =  $cgi->param('action') || '';
my $plan_id = $cgi->param('plan_id');
ThrowUserError("testopia-missing-parameter", {param => "plan_id"}) unless $plan_id;
detaint_natural($plan_id);
validate_test_id($plan_id, 'plan');

my $plan = Bugzilla::Testopia::TestPlan->new($plan_id);
ThrowUserError("testopia-read-only", {'object' => 'Plan'}) unless $plan->canedit;

#########################
### Create a Category ###
#########################
if ($action eq 'add'){
    $vars->{'plan'} = $plan;
    $vars->{'action'} = 'do_add';
    $vars->{'title'} = "Create a New Category for ". $plan->product_name;
    $template->process("testopia/category/form.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());
}

elsif ($action eq 'do_add'){
    my $cname = $cgi->param('name');
    my $desc  = $cgi->param('desc');
    
    # Since these are passed to the database using placeholders,
    # we are safe using trick_taint
    trick_taint($cname);
    trick_taint($desc);

    my $category = Bugzilla::Testopia::Category->new({
                                      product_id  => $plan->product_id,
                                      name        => $cname,
                                      description => $desc
                   });
    ThrowUserError('testopia-name-not-unique', 
                    {'object' => 'Category', 
                     'name' => $cname}) if $category->check_name($cname);
    $category->store;

    $vars->{'tr_message'} = "Category successfully added";
    display($plan);
                                          
   
}

#######################
### Edit a Category ###
#######################
elsif ($action eq 'edit'){
    my $category = Bugzilla::Testopia::Category->new($cgi->param('category_id'));
    $vars->{'plan'} = $plan;
    $vars->{'category'} = $category;
    $vars->{'action'} = 'do_edit';
    $vars->{'title'} = "Edit Category ". $category->name ." for ". $plan->name;
    $template->process("testopia/category/form.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());

}
elsif ($action eq 'do_edit'){
    my $dbh = Bugzilla->dbh;
    my $cname = $cgi->param('name');
    my $desc  = $cgi->param('desc');
    my $cid   = $cgi->param('category_id');
    detaint_natural($cid);
    my $category = Bugzilla::Testopia::Category->new($cid);
    
    # Since these are passed to the database using placeholders,
    # we are safe using trick_taint
    trick_taint($cname);
    trick_taint($desc);

    ThrowUserError('testopia-name-not-unique', 
                    {'object' => 'Category', 
                     'name' => $cname}) if $category->check_name($cname);

    $category->update($cname, $desc);
    $vars->{'tr_message'} = "Category successfully updated";
    display($plan);

}

#########################
### Delete a Category ###
#########################
elsif ($action eq 'delete'){
    my $category = Bugzilla::Testopia::Category->new($cgi->param('category_id'));
    $vars->{'plan'} = $plan;
    $vars->{'category'} = $category;
    $template->process("testopia/category/delete.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());
}
elsif ($action eq 'do_delete'){
    my $category = Bugzilla::Testopia::Category->new($cgi->param('category_id'));
    if ($category->case_count){
        if ($cgi->param('ajax')){
            print "<return><error><text>This category has test cases associated with it. ";
            print "Please move them to another category before deleting this one.</text></error></return>";
            exit;
        } else {
            ThrowUserError("testopia-non-zero-case-count");
        }
    }
    $category->remove($plan);

    # If this is from tr_show_plan.cgi we just need the values
    if ($cgi->param('ajax')){
        my $xml = get_categories_xml($plan);
        print $xml;
        exit;
    }

    $vars->{'tr_message'} = "Category successfully removed";
    display($plan);

}
############################
### View plan Categories ###
############################
else {
    $vars->{'canmanage'} = $plan->canedit;
    $vars->{'plan'} = $plan;
    $template->process("testopia/category/list.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());
}

sub get_categories_xml {
    my ($plan) = @_;
    my $ret = "<items>";
    foreach my $c (@{$plan->categories}){
        $ret .= "<category>";
        $ret .= "<id>". $c->id ."</id>";
        $ret .= "<name>". $c->name ."</name>";
        $ret .= "</category>";
    }
    $ret .= "</items>";
    print STDERR "$ret";
    return $ret;
}

sub display{
    my $plan = shift;
    $vars->{'plan'} = $plan;
    $vars->{'action'} = "update";
    $vars->{'form_action'} = "tr_show_plan.cgi";

    $cgi->delete_all;
    $cgi->param('plan_id', $plan->id); 
    my $casequery = new Bugzilla::CGI($cgi);
    my $runquery  = new Bugzilla::CGI($cgi);

    $casequery->param('current_tab', 'case');
    my $search = Bugzilla::Testopia::Search->new($casequery);
    my $table = Bugzilla::Testopia::Table->new('case', 'tr_show_plan.cgi', $casequery, undef, $search->query);
    $vars->{'case_table'} = $table;    
  
    $runquery->param('current_tab', 'run');
    $search = Bugzilla::Testopia::Search->new($runquery);
    $table = Bugzilla::Testopia::Table->new('run', 'tr_show_plan.cgi', $runquery, undef, $search->query);
    $vars->{'run_table'} = $table;
    $template->process("testopia/plan/show.html.tmpl", $vars)
       || ThrowTemplateError($template->error());
    
}
