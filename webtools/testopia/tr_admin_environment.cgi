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
# Portions created by Greg Hendricks are Copyright (C) 2001
# Greg Hendricks. All Rights Reserved.
#
# Contributor(s): Greg Hendricks <ghendricks@novell.com>
#                 Michael Hight <mjhight@gmail.com>
#                 Garrett Braden <gbraden@novell.com>
#                 Scott Sudweeks <ssudweeks@novell.com>
#                 Brian Kramer <bkramer@novell.com>

use strict;
use lib ".";

use Bugzilla;
use Bugzilla::Util;
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::TestRun;
use Bugzilla::Testopia::Product;
use Bugzilla::Testopia::Classification;
use Bugzilla::Testopia::Environment;
use Bugzilla::Testopia::Environment::Element;
use Bugzilla::Testopia::Environment::Property;
use Bugzilla::Testopia::Environment::Category;

use JSON;
use Data::Dumper;

Bugzilla->login(LOGIN_REQUIRED);
Bugzilla->batch(1);

my $cgi = Bugzilla->cgi;

use vars qw($vars $template);
my $template = Bugzilla->template;

print $cgi->header;

my $action = $cgi->param('action') || '';
my $env_id = $cgi->param('env_id') || 0;

if($action eq 'getChildren'){
    my $json = new JSON;
    my $data = $json->jsonToObj($cgi->param('data'));
    
    my $node = $data->{'node'};
    
    my $id = $node->{'objectId'};
    my $type = $node->{'widgetId'};
    
    detaint_natural($id);
    trick_taint($type);
    
    
    for ($type){
        /classification/ && do { get_products($id);          };
        /product/        && do { get_categories($id);        };
        /category/       && do { get_category_element($id)   };
        /element/        && do { get_element_children($id);  };
        /property/       && do { get_validexp_json($id)      };
    }
}

elsif($action eq 'edit'){
    my $type = $cgi->param('type');
    my $id = $cgi->param('id');
    
    for ($type){
        /category/      && do { edit_category($id); };
        /element/       && do { edit_element($id);  };
        /property/      && do { edit_property($id); };
        /validexp/      && do { edit_validexp($id); };
    }
}

elsif($action eq 'do_edit'){
    my $type = $cgi->param('type');
    my $id = $cgi->param('id');
    
    for ($type){
        /category/      && do { do_edit_category($id); };
        /element/       && do { do_edit_element($id);  };
        /property/      && do { do_edit_property($id); };
        /validexp/      && do { do_edit_validexp($id); };
    }
}

elsif($action eq 'createChild'){
    my $json = new JSON;
    my $data = $json->jsonToObj($cgi->param('data'));
    
    my $node = $data->{'parent'};
    
    my $id = $node->{'objectId'};
    my $type = $node->{'widgetId'};
    
    my $create_type = $data->{'data'}->{'createType'};
    $type = 'child' if ($create_type && $type !~ /category/);
    
    detaint_natural($id);
    trick_taint($type);
    
    
    for ($type){
        /product/       && do { add_category($id);   };
        /category/      && do { add_element($id);    };
        /child/         && do { add_element($id,1);  };
        /element/       && do { add_property($id);   };
        /property/      && do { add_validexp($id);   };
    }
    
}

elsif($action eq 'removeNode'){
    my $json = new JSON;
    my $data = $json->jsonToObj($cgi->param('data'));
    my $node = $data->{'node'};
    
    my $id = $node->{'objectId'};
    my $type = $node->{'widgetId'};
    
    detaint_natural($id) unless $type =~ /validexp/;
    trick_taint($type);
    
    for ($type){
        /category/      && do { delete_category($id); };
        /element/       && do { delete_element($id);  };
        /property/      && do { delete_property($id); };
        /validexp/      && do { delete_validexp($id); };
    }
}

elsif ($action eq 'getcategories'){
    my $product_id = $cgi->param('product_id');
    detaint_natural($product_id);
    my $product = Bugzilla::Testtopia::Product->new($product_id);
    exit unless Bugzilla->user->can_see_product($product->name);
    my $cat = Bugzilla::Testopia::Environment::Category({});
    my $categories = $cat->get_element_categories_by_product($product_id);
    my $ret;
    foreach my $c (@{$categories}){
        $c->name =~ s/<span style='color:blue'>|<\/span>//g;
        $ret .= $c->id.'||'.$c->name.'|||';
    }
    chop($ret);
    print $ret;
}

elsif ($action eq 'getelements'){
    my $cat_name = $cgi->param('cat_name');
    my $cat = Bugzilla::Testopia::Environment::Category->new($cat_name);
    
    my $elements = $cat->get_elements_by_category();
    my $ret;
    foreach my $e (@{$elements}){
        my $elem = Bugzilla::Testopia::Environment::Element->new(@$e{'element_id'});
        $elem->{'name'} =~ s/<span style='color:blue'>|<\/span>//g;
        $ret .= $elem->{'element_id'}.'||'.$elem->{'name'}.'|||';
    }
    $ret = substr($ret, 0, length($ret) - 3);
    print $ret;
}

elsif ($action eq 'getproperties'){
    my $env = Bugzilla::Testopia::Environment->new({});
    my $elem_id = $cgi->param('elem_id');
   
    
    detaint_natural($elem_id);

    my $properties = $env->get_properties_by_element_id($elem_id);
    my $ret;
    foreach my $p (@{$properties}){
        @$p[1] =~ s/<span style='color:blue'>|<\/span>//g;
        $ret .= @$p[0].'||'.@$p[1].'|||';
    }
    chop($ret);
    print $ret;
}

else{
    &display;
}

sub display{
    my $category = Bugzilla::Testopia::Environment::Category->new({'id' => 0});
    if (Param('useclassification')){
        $vars->{'allhaschild'} = $category->get_all_child_count;
        $vars->{'toplevel'} = Bugzilla->user->get_selectable_classifications;
        $vars->{'type'} = 'classification';
    }
    else {
        $vars->{'toplevel'} = $category->get_env_product_list;
        $vars->{'type'} = 'product';
    }
    
    $template->process("testopia/environment/admin/show.html.tmpl", $vars)
        || print $template->error();
}

###########################
### Tree Helper Methods ###
###########################
sub get_products{
    my ($class_id) = (@_);
    my $class = Bugzilla::Testopia::Classification->new($class_id);
    return unless scalar(grep {$class->id eq $class_id} @{Bugzilla->user->get_selectable_classifications});
    print $class->products_to_json;
}

sub get_categories{
    my ($product_id) = (@_);
    my $category = Bugzilla::Testopia::Environment::Category->new({});
    return unless $category->canedit;
    print $category->product_categories_to_json($product_id);
}

sub get_category_element{
    my ($id) = (@_);
    my $category = Bugzilla::Testopia::Environment::Category->new($id);
    return unless $category->canedit;
    print $category->elements_to_json;
} 

sub get_element_children {
    my ($id) = (@_);
    my $element = Bugzilla::Testopia::Environment::Element->new($id);
    return unless $element->canedit;
    print $element->children_to_json;
}

sub get_validexp_json {
    my ($id) = (@_);
    my $property = Bugzilla::Testopia::Environment::Property->new($id);
    print $property->valid_exp_to_json;
}


###########################
### Edit Helper Methods ###
###########################
sub edit_category{
    my ($id) = (@_);
    my $category = Bugzilla::Testopia::Environment::Category->new($id);
    my $product = Bugzilla::Testopia::Product->new($category->product_id());
    return unless Bugzilla->user->can_see_product($product->name);
    $category->{'name'} =~ s/<span style='color:blue'>|<\/span>//g;

    $vars->{'category'} = $category;
    $vars->{'products'} = $category->get_env_product_list;
    $vars->{'currentproduct'} = $product ? $product : {'id' => 0, 'name' => '--ALL--'};
    $template->process("testopia/environment/admin/category.html.tmpl", $vars)
        || print $template->error();
}

sub edit_element{
    my ($id) = (@_);
    my $element = Bugzilla::Testopia::Environment::Element->new($id);
    my $category = Bugzilla::Testopia::Environment::Category->new($element->env_category_id());
    my $product = Bugzilla::Testopia::Product->new($category->product_id());
    return unless $category->canedit;
    $element->{'name'} =~ s/<span style='color:blue'>|<\/span>//g;
    
    $vars->{'element'} = $element;
    $vars->{'products'} = $category->get_env_product_list;
    $vars->{'currentproduct'} = $product ? $product : {'id' => 0, 'name' => '--ALL--'};
    $vars->{'currentcategory'} = $category;
    $vars->{'categories'} = $category->get_element_categories_by_product($product->{'id'});
    unless($element->parent_id() == 0){
        my $parent = Bugzilla::Testopia::Environment::Element->new($element->parent_id());
        $vars->{'currentelement'} = $parent;
    }
    $vars->{'elements'} = $category->get_elements_by_category(); 
    
    $template->process("testopia/environment/admin/element.html.tmpl", $vars)
        || print $template->error();
}

sub edit_property{
    my ($id) = (@_);
    my $property = Bugzilla::Testopia::Environment::Property->new($id);
    my $element = Bugzilla::Testopia::Environment::Element->new($property->element_id());
    return unless $element->canedit;
    my $cat_id = $element->env_category_id();
    my $elmnts = Bugzilla::Testopia::Environment::Category->new($cat_id)->get_elements_by_category();
    
    $vars->{'property'} = $property;
    $vars->{'elements'} = \@$elmnts;
    $vars->{'currentelement'} = $element;
    $template->process("testopia/environment/admin/property.html.tmpl", $vars);
}

sub edit_validexp{
    my ($id) = (@_);
    $id =~ /^(\d+)~/;
    
    my $property = Bugzilla::Testopia::Environment::Property->new($1);
    return unless $property->canedit; 
    
    my @expressions = split /\|/, $property->validexp(); 

    $vars->{'property'} = $property;
    $vars->{'expressions'} = \@expressions;
    $template->process("testopia/environment/admin/valid_exp.html.tmpl", $vars);
}


##############################
### Do_Edit Helper Methods ###
##############################
sub do_edit_category{
    my $name = $cgi->param('name');
    my $product_id = $cgi->param('product');
    my ($id) = (@_);
    my $category = Bugzilla::Testopia::Environment::Category->new($id);
    return unless $category->canedit;
    
    trick_taint($name);
    detaint_natural($product_id);
    
    eval{
        validate_selection($product_id, 'id', 'products');
    };
    if ($@ && $product_id != 0){
        print '{error:"Invalid product"}';
        exit;
    }
    $category->set_product($product_id);
    
    unless ($category->set_name($name)) {
        print '{error:"Name already used. Please choose another"}';
        exit;
    } 
     
    print "{name:\"$name\", product:\"product$product_id\", widget:\"category$id\"}";
    
}

sub do_edit_element{
    my ($id) = (@_);

    #
    # CGI params
    #   productCombo    -> id of product (does not matter what this is, only used to find categories)
    #   categoryCombo   -> id of category (if zero, leave the same)
    #   elementCombo    -> id of parent element (id of 0 means no parent)
    #   name            -> name of the element
    #
    
    my $element = Bugzilla::Testopia::Environment::Element->new($id);
    return unless $element->canedit;
    
    my $cat_id = $cgi->param('categoryCombo');
    my $parent_id = $cgi->param('elementCombo');
    my $name = $cgi->param('name');
    my $parent;
    
    detaint_natural($cat_id);
    detaint_natural($parent_id);
    trick_taint($name);
    
    if ($cat_id){
        $element->update_element_category($cat_id);
        $parent = 'category' . $cat_id;
    }
    else {
        print '{error: "Category does not exist"}';
        exit;
    }
    if ($parent_id){
        $element->update_element_parent($parent_id);
        $parent = 'element' . $parent_id;
    }
    elsif (!$cat_id) {
        print '{error: "Parent element does not exist"}';
        exit;
    }
    unless ($element->update_element_name($name)){
        print '{error: "Name already taken. Please choose another."}';
        exit;
    }
    
    print "{name:\"$name\", parent:\"$parent\", widget:\"element$id\"}";
}

sub do_edit_property{
    my ($id) = (@_);
    my $name = $cgi->param('name');
    my $element_id = $cgi->param('element');
    my $property = Bugzilla::Testopia::Environment::Property->new($id);
    return unless $property->canedit;
    
    trick_taint($name);
    detaint_natural($element_id);
   
    eval{
        validate_selection($element_id, 'element_id', 'test_environment_element');
    };
    if ($@){
        print '{error:"Invalid element"}';
        exit;
    }
    $property->set_element($element_id);
    
    unless ($property->set_name($name)) {
        print '{error:"Name already used. Please choose another"}';
        exit;
    } 
     
    print "{name:\"$name\", element:\"element$element_id\", widget:\"property$id\"}"; 
}

sub do_edit_validexp{
    my ($id) = (@_);
    
    my $property =  Bugzilla::Testopia::Environment::Property->new($id);
    return unless $property->canedit;
    my @expressions =  $cgi->param('valid_exp');
    
    my $exp;
    foreach my $e (@expressions){
        trick_taint($e);
        $exp .= $e.'|';
    }
    
    $property->update_property_validexp($exp);
    display;
}



###################################
### Create Child Helper Methods ###
###################################
sub add_category{
    my ($id) = (@_);
    
    my $category = Bugzilla::Testopia::Environment::Category->new({});
    my $product = Bugzilla::Testopia::Product->new($id);
    return unless Bugzilla->user->can_see_product($product->name);
    $category->{'product_id'} = $id;
    $category->{'name'} = 'New category ' . $category->new_category_count;
    
    my $new_cid = $category->store();
    
    my $json = '{title:"<span style=\'color:blue\'>' . $category->{'name'} . '</span>",';
    $json   .=  'objectId:"'. $new_cid . '",';
    $json   .=  'widgetId:"category' . $new_cid . '",';
    $json   .=  'childIconSrc:"testopia/img/square.gif",';
    $json   .=   'actionsDisabled:["addCategory","addValue","addProperty"]}';
    
    print $json; 
}

sub add_element{
    my ($id, $ischild) = (@_);
    
    my $element = Bugzilla::Testopia::Environment::Element->new({});
    # If we are adding this element as a child, $id is the parent element's id
    if ($ischild) {
        my $parent = Bugzilla::Testopia::Environment::Element->new($id);
        $element->{'env_category_id'} = $parent->env_category_id;
    }
    # Otherwise $id is the catagory id
    else {
        $element->{'env_category_id'} = $id;
    }
    $element->{'name'} = "New element " . $element->new_element_count;
    $element->{'parent_id'} = $ischild ? $id : 0;
    $element->{'isprivate'} = 0; 
    
    my $new_eid = $element->store();
    
    my $json =  '{title:"<span style=\'color:blue\'>' . $element->{'name'} .'</span>",';
    $json   .=   'objectId:"'. $new_eid .'",';
    $json   .=   'widgetId:"element'. $new_eid .'",';
    $json   .=   'childIconSrc:"testopia/img/circle.gif",';
    $json   .=   'actionsDisabled:["addCategory","addValue"]}';
    
    print $json;
}

sub add_property{
    my ($id) = (@_);
    #add new property to element with id=$id
    my $property = Bugzilla::Testopia::Environment::Property->new({});
    $property->{'element_id'} = $id;
    $property->{'name'} = "New property " . $property->new_property_count;
    $property->{'validexp'} = "";
    
    my $new_pid = $property->store();
    
    my $json = '{title:"<span style=\'color:blue\'>' . $property->{'name'} . '</span>",';
    $json   .=  'objectId:"' . $new_pid . '",';
    $json   .=  'widgetId:"property' . $new_pid . '",';
    $json   .=  'childIconSrc:"testopia/img/triangle.gif",';
    $json   .=  'actionsDisabled:["addCategory","addElement","addProperty"]}'; 
 
    print $json;   
}

sub add_validexp{
    my ($id) = (@_);
    my $property = Bugzilla::Testopia::Environment::Property->new($id);
    
    my $exp = $property->validexp;
    $exp ? $property->update_property_validexp($exp . "|New value") : $property->update_property_validexp("New value");
    
    my $json .= '{title: "<span style=\'color:blue\'>New value</span>",';
    $json    .=  'objectId:"' . $id . '~New Value",';
    $json    .=  'widgetId:"validexp' . $id . '~New Value",';
    $json    .=  'actionsDisabled:["addCategory","addElement","addProperty","addValue"]}';

    print $json;
}

#############################
### Delete Helper Methods ###
#############################
sub delete_category{
    my ($id) = (@_);
    my $category = Bugzilla::Testopia::Environment::Category->new($id);
    my $success = $category->obliterate;
    print $success == 1 ? "true" : "false";
}

sub delete_element{
    my ($id) = (@_);
    my $element = Bugzilla::Testopia::Environment::Element->new($id);
    my $success = $element->obliterate;
    print $success == 1 ? "true" : "false";
}

sub delete_property{
    my ($id) = (@_);
    my $property = Bugzilla::Testopia::Environment::Property->new($id);
    my $success = $property->obliterate;
    print $success == 1 ? "true" : "false";
}

sub delete_validexp{
    # $id, $type
    my ($id) = (@_);
    $id =~ /^(\d+)~/;
    my $property = Bugzilla::Testopia::Environment::Property->new($1);
    my %values;
    foreach my $v (split /\|/, $property->validexp){
        $values{$v} = 1;
    }
    $id =~ /~(.+)$/;
    my $value = $1;
    my $deleted = delete $values{$value};
    my $exp = join("|", keys %values);
    $property->update_property_validexp($exp);
    print $deleted ? "true" : "false";
}
