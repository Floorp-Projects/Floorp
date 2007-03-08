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
# Contributor(s): Maciej Maczynski <macmac@xdsnet.pl>
#                 Ed Fuentetaja <efuentetaja@acm.org>
#                 Greg Hendricks <ghendricks@novell.com>

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Util;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::Constants;
use Bugzilla::Testopia::Table;
use Bugzilla::Testopia::TestPlan;
use Bugzilla::Testopia::TestTag;
use Bugzilla::Testopia::Category;
use Bugzilla::Testopia::Attachment;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;
use JSON;

require 'globals.pl';

use vars qw($vars);
my $template = Bugzilla->template;
my $run_query_limit = 5000;
my $case_query_limit = 10000;

Bugzilla->login(LOGIN_REQUIRED);

my $dbh = Bugzilla->dbh;
my $cgi = Bugzilla->cgi;

my $plan_id = trim(Bugzilla->cgi->param('plan_id') || '');
my $action = $cgi->param('action') || '';
unless ($plan_id){
  $vars->{'form_action'} = 'tr_show_plan.cgi';
  print $cgi->header;
  $template->process("testopia/plan/choose.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());
  exit;
}
validate_test_id($plan_id, 'plan');

push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

my $serverpush = support_server_push($cgi);

$vars->{'action'} = "Commit";
$vars->{'fullwidth'} = 1;
$vars->{'form_action'} = "tr_show_plan.cgi";

####################
### Edit Actions ###
####################

### Archive or Unarchive ###
if ($action eq 'Archive' || $action eq 'Unarchive'){
    print $cgi->header;
    my $plan = Bugzilla::Testopia::TestPlan->new($plan_id);
    ThrowUserError("testopia-read-only", {'object' => 'plan'}) unless $plan->canedit;
    do_update($plan);
    $vars->{'plan'} = $plan;
    $plan->toggle_archive(Bugzilla->user->id);
    $vars->{'tr_message'} = 
        $plan->isactive == 0 ? "Plan archived":"Plan Unarchived";
    $vars->{'backlink'} = $plan;
    display($plan);
        
}
#############
### Clone ###
#############
elsif ($action eq 'Clone'){
    print $cgi->header;
    ThrowUserError("testopia-permission-denied", {'object' => 'plan'}) unless Bugzilla->user->in_group('Testers');
    my $plan = Bugzilla::Testopia::TestPlan->new($plan_id);
    do_update($plan);
    $vars->{'plan'} = $plan;
    $template->process("testopia/plan/clone.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());
    
}
elsif ($action eq 'do_clone'){
    unless (Bugzilla->user->in_group('Testers')){
        print $cgi->header;
        ThrowUserError("testopia-permission-denied", {'object' => 'plan'});
    }

    if ($serverpush) {
        print $cgi->multipart_init();
        print $cgi->multipart_start();
    
        $template->process("list/server-push.html.tmpl", $vars)
          || ThrowTemplateError($template->error());

    }
    my $plan = Bugzilla::Testopia::TestPlan->new($plan_id);

    my $plan_name = $cgi->param('plan_name');

    # All DB actions use place holders so we are OK doing this
    my $product_id = $cgi->param('product_id');
    my $version = $cgi->param('prod_version');
    trick_taint($plan_name);
    trick_taint($version);
    detaint_natural($product_id);
    validate_selection($product_id,'id','products');
    Bugzilla::Version::check_version(Bugzilla::Product->new($product_id),$version);
    my $author = $cgi->param('keepauthor') ? $plan->author->id : Bugzilla->user->id;
    my $newplanid = $plan->clone($plan_name, $author, $product_id, $version, $cgi->param('copy_doc'));
    my $newplan = Bugzilla::Testopia::TestPlan->new($newplanid);
    if($cgi->param('copy_tags')){
        foreach my $tag (@{$plan->tags}){
            my $newtag = Bugzilla::Testopia::TestTag->new({
                           tag_name  => $tag->name
                         });
            my $newtagid = $newtag->store;
            $newplan->add_tag($newtagid);
        }
    }
    if($cgi->param('copy_attachments')){
        foreach my $att (@{$plan->attachments}){
            $att->link_plan($newplanid);
        }
    }
    if ($cgi->param('copy_perms')){
        $plan->copy_permissions($newplanid);
        $newplan->derive_regexp_testers($plan->tester_regexp);
    }
    else {
        # Give the author admin rights
        $newplan->add_tester($author, TR_READ | TR_WRITE | TR_DELETE | TR_ADMIN );
        $newplan->set_tester_regexp( Param('testopia-default-plan-testers-regexp'), 3)
            if Param('testopia-default-plan-testers-regexp');
        $newplan->derive_regexp_testers(Param('testopia-default-plan-testers-regexp'))
    } 
    if ($cgi->param('copy_cases')){
        my @case_ids;

        foreach my $id ($cgi->param('clone_categories')){
            detaint_natural($id);
            validate_selection($id,'category_id','test_case_categories');
            my $category = Bugzilla::Testopia::Category->new($id);
            push @case_ids, @{$category->case_ids};
        }
        
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

            my $case = Bugzilla::Testopia::TestCase->new($id);
            # Copy test cases creating new ones
            if ($cgi->param('copy_cases') == 2 ){
                my $caseid = $case->copy($newplan->id, $author, 1);
                my $newcase = Bugzilla::Testopia::TestCase->new($caseid);
                $case->link_plan($newplan->id, $caseid);

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
            # Just create a link
            else {
                $case->link_plan($newplan->id);
            }
        }
    }
    if ($serverpush) {
        print $cgi->multipart_end;
        print $cgi->multipart_start;
    } else {
        print $cgi->header;
    }
    $vars->{'tr_message'} = "Plan ". $plan->name ." cloned as " . $newplan->name .".";
    $vars->{'backlink'} = $plan;
    $cgi->param('plan_id', $newplan->id);
    
    display($newplan);   
    print $cgi->multipart_final if $serverpush; 
}

### Changes to Plan Attributes or Doc ###
elsif ($action eq 'Commit'){
    print $cgi->header;
    my $plan = Bugzilla::Testopia::TestPlan->new($plan_id);
    ThrowUserError("testopia-read-only", {'object' => 'plan'}) unless $plan->canedit;
    do_update($plan);
    $vars->{'tr_message'} = "Test plan updated";
    $vars->{'backlink'} = $plan;
    display($plan);    
}

elsif ($action eq 'Print'){
    my $plan = Bugzilla::Testopia::TestPlan->new($plan_id);
    unless ($plan->canview){
        print $cgi->header;
        ThrowUserError("testopia-permission-denied", {'object' => 'plan'});
    }
    $vars->{'printdoc'} = 1;
    $cgi->param('ctype', 'print');
    display($plan);    
}

elsif ($action eq 'History'){
    print $cgi->header;
    my $plan = Bugzilla::Testopia::TestPlan->new($plan_id);
    ThrowUserError("testopia-permission-denied", {'object' => 'plan'}) unless $plan->canview;
    $vars->{'plan'} = $plan; 
    $vars->{'diff'} = $plan->diff_plan_doc($cgi->param('new'),$cgi->param('old'));
    $vars->{'new'} = $cgi->param('new');
    $vars->{'old'} = $cgi->param('old');
    $template->process("testopia/plan/history.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
       
}
#######################
### Add attachments ###
#######################
elsif ($action eq 'Attach'){
    print $cgi->header;
    my $plan = Bugzilla::Testopia::TestPlan->new($plan_id);
    ThrowUserError("testopia-read-only", {'object' => 'plan'}) unless $plan->canedit;
    defined $cgi->upload('data')
        || ThrowUserError("file_not_specified");
    my $filename = $cgi->upload('data');       
    $cgi->param('description')
        || ThrowUserError("missing_attachment_description");
    my $description = $cgi->param('description');
    my $contenttype = $cgi->uploadInfo($cgi->param('data'))->{'Content-Type'};
    trick_taint($description);
    #trick_taint($contenttype);
    my $fh = $cgi->upload('data');
    my $data;
    # enable 'slurp' mode
    local $/;
    $data = <$fh>;       
    $data || ThrowUserError("zero_length_file");
    
    my $attachment = Bugzilla::Testopia::Attachment->new({
                        plan_id      => $plan_id,
                        submitter_id => Bugzilla->user->id,
                        description  => $description,
                        filename     => $filename,
                        mime_type    => $contenttype,
                        contents     => $data
    });

    $attachment->store;    
    $vars->{'tr_message'} = "Attachment added successfully";
    $vars->{'backlink'} = $plan;
    do_update($plan);
    display(Bugzilla::Testopia::TestPlan->new($plan_id));
}
elsif ($action eq 'Delete'){
    print $cgi->header;
    my $plan = Bugzilla::Testopia::TestPlan->new($plan_id);
    ThrowUserError("testopia-no-delete", {'object' => 'plan'}) unless $plan->candelete;
    $vars->{'plan'} = $plan;
    $template->process("testopia/plan/delete.html.tmpl", $vars) ||
        ThrowTemplateError($template->error());
    
}
elsif ($action eq 'do_delete'){
    my $plan = Bugzilla::Testopia::TestPlan->new($plan_id);
    unless ($plan->candelete){
        print $cgi->header;
        ThrowUserError("testopia-no-delete", {'object' => 'plan'});
    }
    if ($serverpush) {
        print $cgi->multipart_init();
        print $cgi->multipart_start();
        $vars->{'complete'} = 1;
        $vars->{'total'} = 250;
        $template->process("testopia/progress.html.tmpl", $vars)
          || ThrowTemplateError($template->error());

        $plan->obliterate($cgi,$template);
    }
    else {
        $plan->obliterate;
    }

    if ($serverpush) {
        print $cgi->multipart_end;
        print $cgi->multipart_start;
    } else {
        print $cgi->header;
    }
    
    $vars->{'deleted'} = 1;
    $template->process("testopia/plan/delete.html.tmpl", $vars) ||
        ThrowTemplateError($template->error());
    print $cgi->multipart_final if $serverpush;
}
####################
### Just show it ###
####################
else{
    my $plan = Bugzilla::Testopia::TestPlan->new($plan_id);
    ThrowUserError("testopia-permission-denied", {'object' => 'plan'}) unless $plan->canview;
    display($plan);
}
###################
### Helper Subs ###
###################
sub do_update {
    my ($plan) = @_;

    my $newdoc = $cgi->param("plandoc");
    my $plan_name = trim($cgi->param('plan_name')) || '';
    my $product = Bugzilla::Testopia::Product->new($cgi->param('product_id'));
    my $prodver = $cgi->param('prod_version') || '';
    my $type = $cgi->param('type');

    ThrowUserError('testopia-missing-required-field', 
        {'field' => 'product'}) unless $product;
    ThrowUserError('testopia-missing-required-field', 
        {'field' => 'name'}) if ($plan_name eq '');
    ThrowUserError('testopia-missing-required-field', 
        {'field' => 'product version'}) if ($prodver eq '');

    trick_taint($plan_name);
    trick_taint($prodver);

    detaint_natural($type);
    
    validate_selection($type, 'type_id', 'test_plan_types');
    
    my $version = Bugzilla::Version::check_version($product, $prodver);
       
    if($plan->diff_plan_doc($newdoc) ne ''){
        $plan->store_text($plan->id, Bugzilla->user->id, $newdoc);
    }
    
    my %newvalues = ( 
        'name'       => $plan_name,
        'product_id' => $product->id,
        'default_product_version' => $version->name,
        'type_id'    => $type
    );
    
    $plan->update(\%newvalues);
    
    # Add new tags 
    foreach my $tag_name (split(/[,]+/, $cgi->param('newtag'))){
        trick_taint($tag_name);
        my $tag = Bugzilla::Testopia::TestTag->new({tag_name => $tag_name});
        my $tag_id = $tag->store;
        $plan->add_tag($tag_id);
    }
    
    $cgi->delete_all;
    $cgi->param('plan_id', $plan->id);
}

sub display {
    my $plan = shift;
    my $casequery = new Bugzilla::CGI($cgi);
    my $runquery  = new Bugzilla::CGI($cgi);

    if (($cgi->param('order') || $cgi->param('page') || $cgi->param('viewall')) && $cgi->param('current_tab') eq 'case'){
        my $search = Bugzilla::Testopia::Search->new($cgi);
        my $table = Bugzilla::Testopia::Table->new('case', 'tr_show_plan.cgi', $cgi, undef, $search->query);
        ThrowUserError('testopia-query-too-large', {'limit' => $case_query_limit}) if $table->view_count > $case_query_limit;
        
        $vars->{'case_table'} = $table;
        $runquery->delete('order');
        $runquery->delete('page');
        $runquery->delete('viewall');
        $runquery->param('current_tab', 'run');
        $search = Bugzilla::Testopia::Search->new($runquery);
        $table = Bugzilla::Testopia::Table->new('run', 'tr_show_plan.cgi', $runquery, undef, $search->query);
        ThrowUserError('testopia-query-too-large', {'limit' => $run_query_limit}) if $table->view_count > $run_query_limit;
        $vars->{'run_table'} = $table;    

    }
    elsif (($cgi->param('order') || $cgi->param('page') || $cgi->param('viewall')) && $cgi->param('current_tab') eq 'run'){
        my $search = Bugzilla::Testopia::Search->new($cgi);
        my $table = Bugzilla::Testopia::Table->new('run', 'tr_show_plan.cgi', $cgi, undef, $search->query);
        ThrowUserError('testopia-query-too-large', {'limit' => $run_query_limit}) if $table->view_count > $run_query_limit;
        
        $vars->{'run_table'} = $table;
        $casequery->delete('order');
        $casequery->delete('page');
        $casequery->delete('viewall');
        $casequery->param('current_tab', 'case');
        $search = Bugzilla::Testopia::Search->new($casequery);
        $table = Bugzilla::Testopia::Table->new('case', 'tr_show_plan.cgi', $casequery, undef, $search->query);
        ThrowUserError('testopia-query-too-large', {'limit' => $case_query_limit}) if $table->view_count > $case_query_limit;
        $vars->{'case_table'} = $table;    
    }
    else {
    
        $casequery->param('current_tab', 'case');
        my $search = Bugzilla::Testopia::Search->new($casequery);
        my $table = Bugzilla::Testopia::Table->new('case', 'tr_show_plan.cgi', $casequery, undef, $search->query);
        ThrowUserError('testopia-query-too-large', {'limit' => $case_query_limit}) if $table->view_count > $case_query_limit;
        $vars->{'case_table'} = $table;    
      
        $runquery->param('current_tab', 'run');
        $search = Bugzilla::Testopia::Search->new($runquery);
        $table = Bugzilla::Testopia::Table->new('run', 'tr_show_plan.cgi', $runquery, undef, $search->query);
        ThrowUserError('testopia-query-too-large', {'limit' => $run_query_limit}) if $table->view_count > $run_query_limit;
        $vars->{'run_table'} = $table;    
      
    }
    # Dojo will try to parse every tag looking for widgets unless we tell it 
    # which tags have them. 
    my @dojo_search;
    foreach my $run (@{$plan->test_runs}){
        push @dojo_search, "tip_" . $run->id;
    }
    push @dojo_search, "plandoc","newtag","tagTable";
    $vars->{'dojo_search'} = objToJson(\@dojo_search);
    
    $vars->{'plan'} = $plan;
    
    my $format = $template->get_format("testopia/plan/show", scalar $cgi->param('format'), scalar $cgi->param('ctype'));
  	my $disp = "inline";
	# We set CSV files to be downloaded, as they are designed for importing
    # into other programs.
    if ( $format->{'extension'} eq "csv" || $format->{'extension'} eq "xml" )
    {
		$disp = "attachment";
		$vars->{'displaycolumns'} = \@Bugzilla::Testopia::Constants::TESTCASE_EXPORT;
    }
	
	# Suggest a name for the file if the user wants to save it as a file.
    my @time = localtime(time());
    my $date = sprintf "%04d-%02d-%02d", 1900+$time[5],$time[4]+1,$time[3];
	my $filename = "testcases-$date.$format->{extension}";
    print $cgi->header(-type => $format->{'ctype'},
					   -content_disposition => "$disp; filename=$filename") 
					   unless ($action eq 'do_clone');
	
    $vars->{'percentage'} = \&percentage;			   	  
    $template->process($format->{'template'}, $vars) ||
        ThrowTemplateError($template->error());

}
