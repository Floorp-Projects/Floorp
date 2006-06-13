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
use Bugzilla::User;
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::TestTag;
use Bugzilla::Testopia::TestPlan;
use Bugzilla::Testopia::TestRun;
use Bugzilla::Testopia::TestCase;

Bugzilla->login(LOGIN_REQUIRED);

my $cgi = Bugzilla->cgi;

use vars qw($vars $template);
my $template = Bugzilla->template;

print $cgi->header;

my $action   = $cgi->param('action') || '';
my $type     = $cgi->param('type');
my $id = $cgi->param('id');
detaint_natural($id) if $id;

$vars->{'id'} = $id;
$vars->{'type'} = $type;

if ($action eq 'delete'){
    ThrowUserError("testopia-no-delete", {'object' => 'Tag'}) unless UserInGroup("admin") && Param("allow-test-deletion");
    my $tag_id   = $cgi->param('tagid');
    validate_test_id($tag_id, 'tag');
    my $tag = Bugzilla::Testopia::TestTag->new($tag_id);
    $tag->obliterate;
    $vars->{'tr_message'} = "Tag " . $tag->name . " deleted";
    display();
}
    
####################
### Ajax Actions ###
####################
elsif ($action eq 'addtag'){
    my $tag_name = $cgi->param('tag');
    trick_taint($tag_name);
    my $tag = Bugzilla::Testopia::TestTag->new({tag_name => $tag_name});
    my $tag_id = $tag->store;
    my $obj;
    if ($type eq 'plan'){
        $obj = Bugzilla::Testopia::TestPlan->new($id);
    } elsif ($type eq 'case'){
        $obj = Bugzilla::Testopia::TestCase->new($id);
    } elsif($type eq 'run'){
        $obj = Bugzilla::Testopia::TestRun->new($id);
    }
    ThrowUserError('testopia-unkown-object') unless $obj;
    ThrowUserError("testopia-read-only", {'object' => $type}) unless $obj->canedit;
    my $tagged = $obj->add_tag($tag_id);

    if ($tagged) {
        $vars->{'tr_error'} = "Error - This tag is already associated with this $type";
        unless ($cgi->param('method')){
            print $vars->{'tr_error'};
            exit 1;
        }
        display();
    }
    else{
        if ($cgi->param('method')){
            $vars->{'tr_message'} = "Added tag" . $tag->name;
            display();
            exit;
        }
        
        $vars->{'item'} = $obj;
        $vars->{'type'} = $type;
        print "OK";
        $template->process("testopia/tag/table.html.tmpl", $vars)
            || print $template->error();
    }
        
}
elsif ($action eq 'removetag'){
    my $id = $cgi->param('id');
    my $tag_id = $cgi->param('tagid');
    detaint_natural($tag_id);
    my $obj;
    if ($type eq 'plan'){
        $obj = Bugzilla::Testopia::TestPlan->new($id);
    } elsif ($type eq 'case'){
        $obj = Bugzilla::Testopia::TestCase->new($id);
    } elsif($type eq 'run'){
        $obj = Bugzilla::Testopia::TestRun->new($id);
    }
    ThrowUserError('testopia-unkown-object') unless $obj;
    ThrowUserError("testopia-read-only", {'object' => $type}) unless $obj->canedit;
    $obj->remove_tag($tag_id);
    if ($cgi->param('method')){
        $vars->{'tr_message'} = "Removed tag";    
        display();
        exit;
    }
    $vars->{'item'} = $obj;
    $vars->{'type'} = $type;
    print "OK";
    $template->process("testopia/tag/table.html.tmpl", $vars)
        || print $template->error();
}
###################
###     Body    ###
###################
else {
    display();
}
###################
### Subroutines ###
###################
sub case      { $a->{'case_count'} <=> $b->{'case_count'}; }
sub case_desc { $b->{'case_count'} <=> $a->{'case_count'}; }
sub plan      { $a->{'plan_count'} <=> $b->{'plan_count'}; }
sub plan_desc { $b->{'plan_count'} <=> $a->{'plan_count'}; }
sub run       { $a->{'run_count'}  <=> $b->{'run_count'}; }
sub run_desc  { $b->{'run_count'}  <=> $a->{'run_count'}; }
sub name      { $a->{'tag_name'}   cmp $b->{'tag_name'}; }
sub name_desc { $b->{'tag_name'}   cmp $a->{'tag_name'}; }

sub display {
    my $tags;
    my $name = $cgi->param('tag');
    if (defined $name){
        my @tags;
        trick_taint($name);
        push @tags, Bugzilla::Testopia::TestTag->check_name($name);
        $tags = \@tags;
    }
    elsif ($type eq 'plan'){
        my $plan = Bugzilla::Testopia::TestPlan->new($id);
        $tags = $plan->tags;
        $vars->{'title'} = "Tags for Plan: " . $plan->name;
    }
    elsif ($type eq 'case') {
        my $case = Bugzilla::Testopia::TestCase->new($id);
        $tags = $case->tags;
        $vars->{'title'} = "Tags for Case: " . $case->summary;        
    }
    elsif ($type eq 'run') {
        my $run = Bugzilla::Testopia::TestRun->new($id);
        $tags = $run->tags;
        $vars->{'title'} = "Tags for Run: " . $run->summary;
    }
    else {
        my $dbh = Bugzilla->dbh;
        my $desc;
        $tags = $dbh->selectcol_arrayref(
                "SELECT tag_id FROM test_tags 
                 ORDER BY tag_name $desc");
        my @tags;
        foreach my $t (@{$tags}){
            my $tag = Bugzilla::Testopia::TestTag->new($t);
            push @tags, $tag;

            #use Data::Dumper; print Dumper($tag); print "\n\n\n";
        }
        $tags = \@tags;
        $vars->{'title'} = "Tags";
    }
    my @sorted;
    if ($cgi->param('field')){
        if ($cgi->param('field') eq 'c'){
            if (!$cgi->param('desc')){
                @sorted = sort case @{$tags};
                $vars->{'desc'} = 1;
            }
            else {
                @sorted = sort case_desc @{$tags};
            }
        }
        if ($cgi->param('field') eq 'p'){
            if (!$cgi->param('desc')){
                @sorted = sort plan @{$tags};
                $vars->{'desc'} = 1;
            }
            else {
                @sorted = sort plan_desc @{$tags};
            }
        }
        if ($cgi->param('field') eq 'r'){
            if (!$cgi->param('desc')){
                @sorted = sort run @{$tags};
                $vars->{'desc'} = 1;
            }
            else {
                @sorted = sort run_desc @{$tags};
            }
        }
        if ($cgi->param('field') eq 't'){
            if (!$cgi->param('desc')){
                @sorted = sort name @{$tags};
                $vars->{'desc'} = 1;
            }
            else {
                @sorted = sort name_desc @{$tags};
            }
        }
    $tags = \@sorted;
    }
    
    $vars->{'tags'} = $tags;
    $template->process("testopia/tag/list.html.tmpl", $vars)
        || print $template->error();

}
