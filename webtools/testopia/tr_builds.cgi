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
use Bugzilla::Testopia::Build;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;
use Bugzilla::Testopia::Util;

Bugzilla->login(LOGIN_REQUIRED);

my $cgi = Bugzilla->cgi;

print $cgi->header;

use vars qw($vars $template);
my $template = Bugzilla->template;

push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

my $action =  $cgi->param('action');
my $plan_id = $cgi->param('plan_id');

ThrowUserError("testopia-missing-parameter", {param => "plan_id"}) unless $plan_id;
detaint_natural($plan_id);
validate_test_id($plan_id, 'plan');

my $plan = Bugzilla::Testopia::TestPlan->new($plan_id);
ThrowUserError('testopia-read-only', {'object' => 'Plan'}) unless $plan->canedit;

######################
### Create a Build ###
######################
if ($action eq 'add'){
    $vars->{'plan'} = $plan;
    $vars->{'action'} = 'do_add';
    $vars->{'title'} = "Create a New Build for ". $plan->name;
    $template->process("testopia/build/form.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());
}

elsif ($action eq 'do_add'){
    my $cname = $cgi->param('name');
    my $desc  = $cgi->param('desc');
    my $tm    = $cgi->param('milestone');
    
    # Since these are passed to the database using placeholders,
    # we are safe using trick_taint
    trick_taint($cname);
    trick_taint($desc);
    trick_taint($tm);

    my $build = Bugzilla::Testopia::Build->new({
                                      product_id  => $plan->product_id,
                                      name        => $cname,
                                      description => $desc,
                                      milestone   => $tm,
                                      isactive    => $cgi->param('isactive') ? 1 : 0,
                   });
    ThrowUserError('testopia-name-not-unique', 
                    {'object' => 'Build', 
                     'name' => $cname}) if $build->check_name($cname);
    $build->store;

    $vars->{'tr_message'} = "Build successfully added";
    display($plan);
   
}

####################
### Edit a Build ###
####################
elsif ($action eq 'edit'){
    my $build = Bugzilla::Testopia::Build->new($cgi->param('build_id'));
    $vars->{'plan'} = $plan;
    $vars->{'build'} = $build;
    $vars->{'action'} = 'do_edit';
    $vars->{'title'} = "Edit Build ". $build->name ." for ". $plan->name;
    $template->process("testopia/build/form.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());

}
elsif ($action eq 'do_edit'){
    my $dbh = Bugzilla->dbh;
    my $cname = $cgi->param('name');
    my $desc  = $cgi->param('desc');
    my $milestone  = $cgi->param('milestone');
    my $bid   = $cgi->param('build_id');
    
    my $build = Bugzilla::Testopia::Build->new($bid);
    detaint_natural($bid);
    
    # Since these are passed to the database using placeholders,
    # we are safe using trick_taint
    trick_taint($cname);
    trick_taint($desc);
    trick_taint($milestone);
    validate_selection($milestone, 'value', 'milestones');
    
    my $orig_id = $build->check_name($cname);
    
    ThrowUserError('testopia-name-not-unique', 
                  {'object' => 'Build', 
                   'name' => $cname}) if ($orig_id && $orig_id != $bid);
    
    $build->update($cname, $desc, $milestone, $cgi->param('isactive') ? 1 : 0);
    # Search.pm thinks we are searching for a build_id
    $cgi->delete('build_id');
    $vars->{'tr_message'} = "Build successfully updated";
    display($plan);
}

######################
### Delete a Build ###
######################
elsif ($action eq 'delete'){
    my $build = Bugzilla::Testopia::Build->new($cgi->param('build_id'));
    $vars->{'plan'} = $plan;
    $vars->{'build'} = $build;
    $template->process("testopia/build/delete.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());

}
elsif ($action eq 'do_delete'){
    my $build = Bugzilla::Testopia::Build->new($cgi->param('build_id'));
    if ($build->case_run_count){
        if ($cgi->param('ajax')){
            print "<return><error><text>This Build has test runs associated with it.";
            print "It can not be removed</text></error></return>";
            exit;
        } else {
            ThrowUserError("testopia-non-zero-run-count", {'object' => 'Build'});
        }
    }

    $build->remove($plan);

    # If this is from tr_show_plan.cgi we just need the values
    if ($cgi->param('ajax')){
        my $xml = get_builds_xml($plan);
        print $xml;
        exit;
    }

    $vars->{'tr_message'} = "Build successfully removed";    
    display($plan);

}
########################
### View plan Builds ###
########################
else {
    display($plan);
}

###################
### Helper Subs ###
###################
sub get_builds_xml {
    my ($plan) = @_;
    my $ret = "<items>";
    foreach my $c (@{$plan->product->builds}){
        $ret .= "<build>";
        $ret .= "<id>". $c->id ."</id>";
        $ret .= "<name>". $c->name ."</name>";
        $ret .= "</build>";
    }
    $ret .= "</items>";
    return $ret;
}

sub display{
    my $plan = shift;
    $vars->{'plan'} = $plan;
    $template->process("testopia/build/list.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());    
}
