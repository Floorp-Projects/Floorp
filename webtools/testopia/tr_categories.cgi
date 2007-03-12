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

use vars qw($vars);
my $template = Bugzilla->template;
my $cgi = Bugzilla->cgi;

print $cgi->header;

push @{$::vars->{'style_urls'}}, 'testopia/css/default.css';

my $action =  $cgi->param('action') || '';
my $product_id = $cgi->param('product_id');

ThrowUserError("testopia-missing-parameter", {param => "product_id"}) unless $product_id;
my $product = Bugzilla::Testopia::Product->new($product_id);
ThrowUserError('testopia-read-only', {'object' => 'Category'}) unless $product->canedit;

$vars->{'plan_id'} = $cgi->param('plan_id');
$vars->{'product'} = $product;  

#########################
### Create a Category ###
#########################
if ($action eq 'add'){
    $vars->{'action'} = 'do_add';
    $template->process("testopia/category/form.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());
}

elsif ($action eq 'do_add'){
    my $cname = $cgi->param('name');
    my $desc  = $cgi->param('desc');
    
    trick_taint($cname);
    trick_taint($desc);

    my $category = Bugzilla::Testopia::Category->new({
                                      product_id  => $product->id,
                                      name        => $cname,
                                      description => $desc
                   });
    ThrowUserError('testopia-name-not-unique', 
                    {'object' => 'Category', 
                     'name' => $cname}) if $category->check_name($cname);
    $category->store;

    $vars->{'tr_message'} = "Category successfully added";
    display();                                        
}

#######################
### Edit a Category ###
#######################
elsif ($action eq 'edit'){
    my $category = Bugzilla::Testopia::Category->new($cgi->param('category_id'));
    $vars->{'category'} = $category;
    $vars->{'action'} = 'do_edit';
    $template->process("testopia/category/form.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());

}
elsif ($action eq 'do_edit'){
    my $cname = $cgi->param('name');
    my $desc  = $cgi->param('desc');
    my $cid   = $cgi->param('category_id');
    my $category = Bugzilla::Testopia::Category->new($cid);
    
    trick_taint($cname);
    trick_taint($desc);
    
    my $orig_id = $category->check_name($cname);
    
    ThrowUserError('testopia-name-not-unique', 
                    {'object' => 'Category', 
                     'name' => $cname}) if ($orig_id && $cid != $orig_id);

    $category->update($cname, $desc);
    $vars->{'tr_message'} = "Category successfully updated";
    display();

}

#########################
### Delete a Category ###
#########################
elsif ($action eq 'delete'){
    my $category = Bugzilla::Testopia::Category->new($cgi->param('category_id'));
    ThrowUserError("testopia-non-zero-case-count") unless $category->candelete;

    $vars->{'category'} = $category;
    $template->process("testopia/category/delete.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());
}
elsif ($action eq 'do_delete'){
    my $category = Bugzilla::Testopia::Category->new($cgi->param('category_id'));
    ThrowUserError("testopia-non-zero-case-count") unless $category->candelete;
    
    $category->remove;

    $vars->{'tr_message'} = "Category successfully removed";
    display();

}
############################
### View plan Categories ###
############################
else {
    display();
}


sub display{
    $template->process("testopia/category/list.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());
}
