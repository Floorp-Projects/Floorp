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
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Util;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::TestCase;
use Bugzilla::Testopia::TestCaseRun;
use Bugzilla::Testopia::Attachment;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;

use vars qw($template $vars);

require "globals.pl";

Bugzilla->login();
print Bugzilla->cgi->header();
   
my $dbh = Bugzilla->dbh;
my $cgi = Bugzilla->cgi;

my $case_id = trim(Bugzilla->cgi->param('case_id')) || '';

unless ($case_id){
  $template->process("testopia/case/choose.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());
  exit;
}
validate_test_id($case_id, 'case');
$vars->{'action'} = "Commit";
$vars->{'form_action'} = "tr_show_case.cgi";

my $action = $cgi->param('action') || '';

if ($action eq 'Clone'){
    Bugzilla->login(LOGIN_REQUIRED);
    my $case = Bugzilla::Testopia::TestCase->new($case_id);
    ThrowUserError("testopia-create-denied", {'object' => 'Test Case'}) unless $case->canedit;
    do_update($case);
    $vars->{'case'} = $case;
    $template->process("testopia/case/clone.html.tmpl", $vars) ||
        ThrowTemplateError($template->error());

}

elsif ($action eq 'do_clone'){
    Bugzilla->login(LOGIN_REQUIRED);
    my $case = Bugzilla::Testopia::TestCase->new($case_id);
    ThrowUserError("testopia-create-denied", {'object' => 'Test Case'}) unless $case->canedit;
    my $count = 0;
    my $method;
    if ($cgi->param('copymethod') eq 'copy'){
        ThowUserError('missing-plans-list') if (!$cgi->param('existing') && !$cgi->param('newplans'));
        my @planids;
        my %planseen;
        foreach my $p (split('[\s,]+', $cgi->param('newplans'))){
            validate_test_id($p, 'plan');
            $planseen{$p} = 1;
        }
        push @planids, keys %planseen;
        if ($cgi->param('existing')){
            foreach my $p (@{$case->plans}){
                push @planids, $p->id;
            }
        }
        my $newcase;
        foreach my $pid (@planids){
            $count++;
            my $newcaseid = $case->copy($pid, $cgi->param('copy_doc'));
            $case->link_plan($pid, $newcaseid);
            $newcase = Bugzilla::Testopia::TestCase->new($newcaseid);
            if ($cgi->param('copy_tags')){
                foreach my $tag (@{$case->tags}){
                    # Doing it this way avoids collisions
                    my $newtag = Bugzilla::Testopia::TestTag->new({
                                   tag_name  => $tag->name
                                 });
                    my $newtagid = $newtag->store;
                    $newcase->add_tag($newtagid);
                }
            }
            if ($cgi->param('copy_comps')){
                foreach my $comp (@{$case->components}){
                    $newcase->add_component($comp->{'id'});
                }
            }
        }
        $method = "copied";
        $vars->{'copied'} = $case;
        $case = $newcase;
    }
    elsif ($cgi->param('copymethod') eq 'link'){
        # This should be a code error
        ThrowUserError('testopia-missing-plans-list') if (!$cgi->param('linkplans'));
        my %seen;
        foreach my $p (split('[\s,]+', $cgi->param('linkplans'))){
            validate_test_id($p, 'plan');
            $seen{$p} = 1;
        }
        foreach my $p (keys %seen){
            $count++;
            $case->link_plan($p);
        }
        delete $case->{'plans'};
        $method = "linked";
    }
            
    $vars->{'tr_message'} = "Case $method to $count plans";
    display($case);
}

elsif ($action eq 'Attach'){
    Bugzilla->login(LOGIN_REQUIRED);
    my $case = Bugzilla::Testopia::TestCase->new($case_id);
    ThrowUserError("testopia-read-only", {'object' => 'case'}) unless $case->canedit;

    defined $cgi->upload('data')
        || ThrowUserError("file_not_specified");
    my $filename = $cgi->upload('data');       
    $cgi->param('description')
        || ThrowUserError("missing_attachment_description");
    my $description = $cgi->param('description');
    my $contenttype = $cgi->uploadInfo($cgi->param('data'))->{'Content-Type'};
    trick_taint($description);
    my $fh = $cgi->upload('data');
    my $data;
    # enable 'slurp' mode
    local $/;
    $data = <$fh>;       
    $data || ThrowUserError("zero_length_file");
    
    my $attachment = Bugzilla::Testopia::Attachment->new({
                        case_id      => $case_id,
                        submitter_id => Bugzilla->user->id,
                        description  => $description,
                        filename     => $filename,
                        mime_type    => $contenttype,
                        contents     => $data
    });

    $attachment->store;
    do_update($case);
    display($case);
}
elsif ($action eq 'Commit'){
    Bugzilla->login(LOGIN_REQUIRED);
    my $case = Bugzilla::Testopia::TestCase->new($case_id);
    ThrowUserError("testopia-read-only", {'object' => 'case'}) unless $case->canedit;
    do_update($case);
    $vars->{'tr_message'} = "Test case updated";
    display($case);
}

elsif ($action eq 'History'){
    my $case = Bugzilla::Testopia::TestCase->new($case_id);
    ThrowUserError("testopia-permission-denied", {'object' => 'case'}) unless $case->canview;
    $vars->{'case'} = $case; 
    $vars->{'diff'} = $case->compare_doc_versions($cgi->param('new'),$cgi->param('old'));
    $vars->{'new'} = $cgi->param('new');
    $vars->{'old'} = $cgi->param('old');
    $template->process("testopia/case/history.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
       
}

####################
### Ajax Actions ###
####################
elsif ($action eq 'addcomponent' || $action eq 'removecomponent'){
    Bugzilla->login(LOGIN_REQUIRED);
    my $case = Bugzilla::Testopia::TestCase->new($case_id);
    ThrowUserError("testopia-read-only", {'object' => 'case'}) unless $case->canedit;
    my $comp = $cgi->param('component_id');
    detaint_natural($comp);
    validate_selection($comp, 'id', 'components');
    
    if ($action eq 'addcomponent'){
        $case->add_component($comp);
    }
    else {
        $case->remove_component($comp);
    }

    my $xml = get_components_xml($case);
    print $xml;   
}

#TODO: Clean up styles and put them in skins
else{
    my $case = Bugzilla::Testopia::TestCase->new($case_id);
    ThrowUserError("testopia-permission-denied", {'object' => 'case'}) unless $case->canview;
    display($case);
}

#######################
### Helper Routines ###
#######################

sub get_components_xml {
    my ($case) = @_;
    my $ret = "<components>";
    foreach my $c (@{$case->components}){
        $ret .= "<selected>";
        $ret .= "<id>". $c->{'id'} ."</id>";
        $ret .= "<name>". $c->{'name'} ."</name>";
        $ret .= "</selected>";
    }

    foreach my $c (@{$case->get_selectable_components}){
        $ret .= "<nonselected>";
        $ret .= "<id>". $c->{'id'} ."</id>";
        $ret .= "<name>". $c->{'name'} ."</name>";
        $ret .= "</nonselected>";
    }
  
    $ret .= "</components>";
    return $ret;
}

sub do_update{
    my ($case) = @_;
    my $newtcaction = $cgi->param("tcaction");
    my $newtceffect = $cgi->param("tceffect");
    my $alias       = $cgi->param('alias')|| '';
    my $category    = $cgi->param('category');
    my $status      = $cgi->param('status');
    my $priority    = $cgi->param('priority');
    my $isautomated = $cgi->param("isautomated");
    my $script      = $cgi->param("script")|| '';
    my $arguments   = $cgi->param("arguments")|| '';    
    my $summary     = $cgi->param("summary")|| '';
    my $requirement = $cgi->param("requirement")|| '';
    my $tcdependson = $cgi->param("tcdependson")|| '';
    my $tcblocks    = $cgi->param("tcblocks")|| '';
    my $tester      = $cgi->param("tester") || '';
    if ($tester){
        $tester = DBNameToIdAndCheck($cgi->param('tester'));
    }

    ThrowUserError('testopia-missing-required-field', {'field' => 'summary'})  if $summary  eq '';
    ThrowUserError('testopia-missing-required-field', {'field' => 'Case Action'}) if $newtcaction eq '';
    ThrowUserError('testopia-missing-required-field', {'field' => 'Case Effect'}) if $newtceffect eq '';
    
    detaint_natural($status);
    detaint_natural($category);
    detaint_natural($priority);
    detaint_natural($isautomated);

    # All inserts are done with placeholders so this is OK
    trick_taint($alias);
    trick_taint($script);
    trick_taint($arguments);
    trick_taint($summary);
    trick_taint($requirement);
    trick_taint($newtcaction);
    trick_taint($newtceffect);
    trick_taint($tcdependson);
    trick_taint($tcblocks);

    validate_selection($category, 'category_id', 'test_case_categories');
    validate_selection($status, 'case_status_id', 'test_case_status');
    
    ThrowUserError('testiopia-alias-exists', 
        {'alias' => $alias}) if $case->check_alias($alias);
    ThrowUserError('testiopia-invalid-data', 
        {'field' => 'isautomated', 'value' => $isautomated }) 
            if ($isautomated !~ /^[01]$/);
            
    if($case->diff_case_doc($newtcaction, $newtceffect) ne ''){
        $case->store_text($case->id, Bugzilla->user->id, $newtcaction, $newtceffect);
    }

    my %newvalues = ( 
        'case_status_id' => $status,
        'category_id'    => $category,
        'priority_id'    => $priority,
        'summary'        => $summary,
        'isautomated'    => $isautomated,
        'alias'          => $alias,
        'requirement'    => $requirement,
        'script'         => $script,
        'arguments'      => $arguments,
        'default_tester_id' => $tester,
    );
    $case->update(\%newvalues);
    $case->update_deps($cgi->param('tcdependson'), $cgi->param('tcblocks'));
    $cgi->delete_all;
    $cgi->param('case_id', $case->id);
}

sub display {
    my $case = shift;    

    $cgi->delete('build');
    $cgi->delete('isautomated');
    $cgi->delete('category');
    $cgi->param('current_tab', 'case_run');
    my $search = Bugzilla::Testopia::Search->new($cgi);
    my $table = Bugzilla::Testopia::Table->new('case_run', 'tr_show_case.cgi', $cgi, undef, $search->query);
    $vars->{'case'} = $case;
    $vars->{'table'} = $table;
    $template->process("testopia/case/show.html.tmpl", $vars) ||
        ThrowTemplateError($template->error());
}