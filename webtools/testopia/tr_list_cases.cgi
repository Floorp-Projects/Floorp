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
# Contributor(s):  Greg Hendricks <ghendricks@novell.com>

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Constants;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::TestCase;
use Bugzilla::Testopia::TestCaseRun;
use Bugzilla::Testopia::TestPlan;
use Bugzilla::Testopia::TestTag;
use Bugzilla::Testopia::Table;

use vars qw($vars $template);
require "globals.pl";

my $dbh = Bugzilla->dbh;
my $cgi = Bugzilla->cgi;
my $template = Bugzilla->template;
my $query_limit = 10000;

push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

$cgi->send_cookie(-name => "TEST_LAST_ORDER",
                  -value => $cgi->param('order'),
                  -expires => "Fri, 01-Jan-2038 00:00:00 GMT");
Bugzilla->login();

# Determine the format in which the user would like to receive the output.
# Uses the default format if the user did not specify an output format;
# otherwise validates the user's choice against the list of available formats.
my $format = $template->get_format("testopia/case/list", scalar $cgi->param('format'), scalar $cgi->param('ctype'));
       
my $action = $cgi->param('action') || '';
my $serverpush = ( support_server_push($cgi) ) && ( $format->{'extension'} eq "html" );

if ($serverpush) {
    print $cgi->multipart_init;
    print $cgi->multipart_start;

    $template->process("list/server-push.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

# prevent DOS attacks from multiple refreshes of large data
$::SIG{TERM} = 'DEFAULT';
$::SIG{PIPE} = 'DEFAULT';

###############
### Actions ###
###############
if ($action eq 'Commit'){
    Bugzilla->login(LOGIN_REQUIRED);

    # Match the list of checked items. 
    my $reg = qr/c_([\d]+)/;
    my $params = join(" ", $cgi->param());
    my @params = $cgi->param();

    unless ($params =~ $reg){
        print $cgi->multipart_end if $serverpush;
        ThrowUserError('testopia-none-selected', {'object' => 'case'});
    }

    my $progress_interval = 250;
    my $i = 0;
    my $total = scalar @params;

    foreach my $p ($cgi->param()){
        my $case = Bugzilla::Testopia::TestCase->new($1) if $p =~ $reg;
        next unless $case;
        
        unless ($case->canedit){
            print $cgi->multipart_end if $serverpush;
            ThrowUserError("testopia-read-only", {'object' => 'case', 'id' => $case->id});
        }
        
        $i++;
        if ($i % $progress_interval == 0 && $serverpush){
            print $cgi->multipart_end;
            print $cgi->multipart_start;
            $vars->{'complete'} = $i;
            $vars->{'total'} = $total;
            $template->process("testopia/progress.html.tmpl", $vars)
              || ThrowTemplateError($template->error());
        } 

        my $requirement = $cgi->param('requirement') eq '--Do Not Change--' ? $case->requirement : $cgi->param('requirement');
        my $arguments = $cgi->param('arguments') eq '--Do Not Change--' ? $case->arguments : $cgi->param('requirement');
        my $script = $cgi->param('script') eq '--Do Not Change--' ? $case->script : $cgi->param('requirement');
        my $status = $cgi->param('status')     == -1 ? $case->status_id : $cgi->param('status');
        my $priority = $cgi->param('priority') == -1 ? $case->{'priority_id'} : $cgi->param('priority');
        my $category = $cgi->param('category') == -1 ? $case->{'category_id'} : $cgi->param('category');
        my $isautomated = $cgi->param('isautomated') == -1 ? $case->isautomated : $cgi->param('isautomated');
        my @comps       = $cgi->param("components");
        my $tester = $cgi->param('tester') || ''; 
        if ($tester && $tester ne '--Do Not Change--'){
            $tester = DBNameToIdAndCheck(trim($cgi->param('tester')));
        }
        else {
            $tester = $case->default_tester->id;
        }
            
        # We use placeholders so trick_taint is ok.
        trick_taint($requirement) if $requirement;
        trick_taint($arguments) if $arguments;
        trick_taint($script) if $script;
        
        detaint_natural($status);
        detaint_natural($priority);
        detaint_natural($category);
        detaint_natural($isautomated);

        my @components;
        foreach my $id (@comps){
            detaint_natural($id);
            validate_selection($id, 'id', 'components');
            push @components, $id;
        }

        my %newvalues = ( 
            'case_status_id' => $status,
            'category_id'    => $category,
            'priority_id'    => $priority,
            'isautomated'    => $isautomated,
            'requirement'    => $requirement,
            'script'         => $script,
            'arguments'      => $arguments,
            'default_tester_id' => $tester  || $case->default_tester->id,
        );
      
        $case->update(\%newvalues);
        $case->add_component($_) foreach (@components);
        if ($cgi->param('addtags')){
            foreach my $name (split(/[,]+/, $cgi->param('addtags'))){
                trick_taint($name);
                my $tag = Bugzilla::Testopia::TestTag->new({'tag_name' => $name});
                my $tag_id = $tag->store;
                $case->add_tag($tag_id);
            }
        }
        # Add to runs
        my @runs;
        foreach my $runid (split(/[\s,]+/, $cgi->param('addruns'))){
            validate_test_id($runid, 'run');
            push @runs, Bugzilla::Testopia::TestRun->new($runid);
        }
        foreach my $run (@runs){
            $run->add_case_run($case->id);
        }
        
        # Clone
        my %planseen;
        foreach my $planid (split(",", $cgi->param('linkplans'))){
            validate_test_id($planid, 'plan');
            $planseen{$planid} = 1;
        }
        if ($cgi->param('copymethod') eq 'copy'){
            foreach my $planid (keys %planseen){
                my $author = $cgi->param('newauthor') ? Bugzilla->user->id : $case->author->id;
                my $newcaseid = $case->copy($planid, $author, 1);
                $case->link_plan($planid, $newcaseid);
                my $newcase = Bugzilla::Testopia::TestCase->new($newcaseid);
                foreach my $tag (@{$case->tags}){
                    # Doing it this way avoids collisions
                    my $newtag = Bugzilla::Testopia::TestTag->new({
                                   tag_name  => $tag->name
                                 });
                    my $newtagid = $newtag->store;
                    $newcase->add_tag($newtagid);
                }
                foreach my $comp (@{$case->components}){
                    $newcase->add_component($comp->{'id'});
                }
            }
        }
        elsif ($cgi->param('copymethod') eq 'link'){
            foreach my $planid (keys %planseen){
                $case->link_plan($planid);
            }
        }
        
    }
    if ($serverpush && !$cgi->param('debug')) {
        print $cgi->multipart_end;
        print $cgi->multipart_start;
    }    
    my @runlist = split(/[\s,]+/, $cgi->param('addruns'));
    if (scalar @runlist == 1){
        my $run_id = $cgi->param('addruns');
        validate_test_id($run_id, 'run');
        $cgi->delete_all;
        $cgi->param('run_id', $run_id);
        $cgi->param('current_tab', 'case_run');
        my $search = Bugzilla::Testopia::Search->new($cgi);
        my $table = Bugzilla::Testopia::Table->new('case_run', 'tr_show_run.cgi', $cgi, undef, $search->query);
        
        my @case_list;
        foreach my $caserun (@{$table->list}){
            push @case_list, $caserun->case_id;
        }
        $vars->{'run'} = Bugzilla::Testopia::TestRun->new($run_id);
        $vars->{'table'} = $table;
        $vars->{'case_list'} = join(",", @case_list);
        $vars->{'action'} = 'Commit';
        $template->process("testopia/run/show.html.tmpl", $vars)
            || ThrowTemplateError($template->error());
        exit;
    } 
    my $case = Bugzilla::Testopia::TestCase->new({ 'case_id' => 0 });
    $vars->{'case'} = $case;
    $vars->{'title'} = "Update Successful";
    $vars->{'tr_message'} = "$i Test Cases Updated";
    $vars->{'current_tab'} = 'case';
    
    $template->process("testopia/search/advanced.html.tmpl", $vars)
        || ThrowTemplateError($template->error()); 
    print $cgi->multipart_final if $serverpush;
    exit;

}
elsif ($action eq 'Delete Selected'){
    Bugzilla->login(LOGIN_REQUIRED);

    # Match the list of checked items. 
    my $reg = qr/c_([\d]+)/;
    my $params = join(" ", $cgi->param());
    my @params = $cgi->param();

    unless ($params =~ $reg){
        print $cgi->multipart_end if $serverpush;
        ThrowUserError('testopia-none-selected', {'object' => 'case'});
    }
    my @deletable;
    my @undeletable;
    foreach my $p ($cgi->param()){
        my $case = Bugzilla::Testopia::TestCase->new($1) if $p =~ $reg;
        next unless $case;
        
        if ($case->candelete){
            push @deletable, $case;
        }
        else {
            push @undeletable, $case;
        }
    }
    print $cgi->multipart_end if $serverpush;
    $vars->{'delete_list'} = \@deletable;
    $vars->{'unable_list'} = \@undeletable;
    $template->process("testopia/case/delete-list.html.tmpl", $vars)
        || ThrowTemplateError($template->error());
    print $cgi->multipart_final if $serverpush;
    exit; 
    
}

elsif ($action eq 'do_delete'){
    Bugzilla->login(LOGIN_REQUIRED);
    my @case_ids = split(",", $cgi->param('case_list'));
    my $progress_interval = 250;
    my $i = 0;
    my $total = scalar @case_ids;
    
    foreach my $id (@case_ids){
        
        $i++;
        if ($i % $progress_interval == 0 && $serverpush){
            print $cgi->multipart_end;
            print $cgi->multipart_start;
            $vars->{'complete'} = $i;
            $vars->{'total'} = $total;
            $template->process("testopia/progress.html.tmpl", $vars)
              || ThrowTemplateError($template->error());
        }
        
        detaint_natural($id);
        next unless $id;
        my $case = Bugzilla::Testopia::TestCase->new($id);
        next unless $case->candelete;
        $case->obliterate;
    }
    
    print $cgi->multipart_end if $serverpush;
    my $case = Bugzilla::Testopia::TestCase->new({});
    $vars->{'case'} = $case;
    $vars->{'title'} = "Update Successful";
    $vars->{'tr_message'} = "$i test cases deleted";
    $vars->{'current_tab'} = 'case';
    
    $template->process("testopia/search/advanced.html.tmpl", $vars)
        || ThrowTemplateError($template->error()); 
    print $cgi->multipart_final if $serverpush;
    exit;
}

###############
### Display ###
###############

$vars->{'qname'} = $cgi->param('qname') if $cgi->param('qname');

# Take the search from the URL params and convert it to SQL
$cgi->param('current_tab', 'case');
my $search = Bugzilla::Testopia::Search->new($cgi);
my $table = Bugzilla::Testopia::Table->new('case', 'tr_list_cases.cgi', $cgi, undef, $search->query);
if ($table->view_count > $query_limit){
    print $cgi->multipart_end if $serverpush;
    ThrowUserError('testopia-query-too-large', {'limit' => $query_limit});
}
# Check that all of the test cases returned only belong to one product.
if ($table->list_count > 0 && !$cgi->param('addrun')){
    my %case_prods;
    my $prod_id;
    foreach my $case (@{$table->list}){
        $case_prods{$case->id} = $case->get_product_ids;
        $prod_id = @{$case_prods{$case->id}}[0];
        if (scalar(@{$case_prods{$case->id}} > 1)){
            $vars->{'multiprod'} = 1 ;
            last;
        }
    }
    # Check that all of them are the same product
    if (!$vars->{'multiprod'}){
        foreach my $c (keys %case_prods){
            if ($case_prods{$c}->[0] != $prod_id){
                $vars->{'multiprod'} = 1;
                last;
            }
        }
    }
    if (!$vars->{'multiprod'}) {
        my $category_list = $table->list->[0]->get_category_list;
        unshift @{$category_list},  {'id' => -1, 'name' => "--Do Not Change--"};
        $vars->{'category_list'} = $category_list;
    }
}
# create an empty case to use for getting status and priority lists
my $c = Bugzilla::Testopia::TestCase->new({'case_id' => 0 });
my $status_list   = $c->get_status_list;
my $priority_list = $c->get_priority_list;

# add the "do not change" option to each list
# we use unshift so they show at the top of the list
unshift @{$status_list},   {'id' => -1, 'name' => "--Do Not Change--"};
unshift @{$priority_list}, {'id' => -1, 'name' => "--Do Not Change--"};

my $addrun = $cgi->param('addrun');
if ($addrun){
    validate_test_id($addrun, 'run');
    my $run = Bugzilla::Testopia::TestRun->new($addrun);
    $vars->{'addruns'} = $addrun;
    $vars->{'plan'} = $run->plan;
}
    
$vars->{'addrun'} = $cgi->param('addrun');
$vars->{'fullwidth'} = 1; #novellonly
$vars->{'case'} = $c;
$vars->{'status_list'} = $status_list;
$vars->{'priority_list'} = $priority_list;
$vars->{'dotweak'} = UserInGroup('Testers');
$vars->{'table'} = $table;
$vars->{'urlquerypart'} = $cgi->canonicalise_query('cmdtype');

my $contenttype;

if ($format->{'extension'} eq "html") {
    $contenttype = "text/html";
}
else {
    $contenttype = $format->{'ctype'};
}

if ($serverpush && !$cgi->param('debug')) {
    print $cgi->multipart_end;
    print $cgi->multipart_start;
}                              
else {
	my @time = localtime(time());
	my $date = sprintf "%04d-%02d-%02d", 1900+$time[5],$time[4]+1,$time[3];
	my $filename = "testcases-$date.$format->{extension}";
	
	my $disp = "inline";
	# We set CSV files to be downloaded, as they are designed for importing
    # into other programs.
    if ( $format->{'extension'} eq "csv" || $format->{'extension'} eq "xml" )
    {
		$disp = "attachment";
		$vars->{'displaycolumns'} = \@Bugzilla::Testopia::Constants::TESTCASE_EXPORT;
    }

    # Suggest a name for the bug list if the user wants to save it as a file.
    print $cgi->header(-type => $contenttype,
					   -content_disposition => "$disp; filename=$filename");
} 
                                       
$template->process($format->{'template'}, $vars)
    || ThrowTemplateError($template->error());
    
print $cgi->multipart_final if $serverpush;
