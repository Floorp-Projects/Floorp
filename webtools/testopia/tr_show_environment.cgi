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
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Testopia::Search;
use Bugzilla::Testopia::Table;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::TestRun;
use Bugzilla::Testopia::Product;
use Bugzilla::Testopia::Classification;
use Bugzilla::Testopia::Environment;
use Bugzilla::Testopia::Environment::Element;
use Bugzilla::Testopia::Environment::Category;
use Bugzilla::Testopia::Environment::Property;
use Data::Dumper;
use JSON;

Bugzilla->login(LOGIN_REQUIRED);

my $cgi = Bugzilla->cgi;

use vars qw($vars $template);
my $template = Bugzilla->template;

print $cgi->header;

my $action = $cgi->param('action') || '';
my $env_id = trim(Bugzilla->cgi->param('env_id')) || '';

unless ($env_id || $action){
  $template->process("testopia/environment/choose.html.tmpl", $vars) 
      || ThrowTemplateError($template->error());
  exit;
}

Bugzilla->batch(1);

if ($action eq 'delete'){
    my $env = Bugzilla::Testopia::Environment->new($env_id);
    ThrowUserError('testopia-no-delete', {'object' => 'Environment'}) unless $env->candelete;
    $vars->{'environment'} = $env;
    $template->process("testopia/environment/delete.html.tmpl", $vars)
        || print $template->error();

}

elsif ($action eq 'do_delete'){
    my $env = Bugzilla::Testopia::Environment->new($env_id);
    ThrowUserError('testopia-no-delete', {'object' => 'Environment'}) unless $env->candelete;
    ThrowUserError("testopia-non-zero-run-count", {'object' => 'Environment'}) if $env->get_run_count;
    $env->obliterate;
    $vars->{'tr_message'} = "Environment Deleted";
    display_list();
}

####################
### Ajax Actions ###
####################

elsif ($action eq 'edit'){
    my $name = $cgi->param('name');
    my $product_id = $cgi->param('product_id');
    
    trick_taint($name);
    detaint_natural($product_id);
    eval{
        validate_selection( $product_id, 'id', 'products');
    };
    if ($@){
        print "{error: 'Invalid product'}";
        exit;
    }
        
    my $env = Bugzilla::Testopia::Environment->new($env_id);
    unless ($env->update({'product_id' => $product_id})){
        print "{error: 'Error updating product'}";
        exit;
    }
    unless ($env->update({'name' => $name})){
        print "{error: 'Name already in use in this product, please choose another'}";
        exit;
    }
    unless ($env->canedit){
        print "{error: 'You don't have rights to edit this environment'}";
        exit;
    }
    print "{message: 'Environment updated'}";
    
}

elsif ($action eq 'getChildren'){
    my $json = new JSON;
    print STDERR $cgi->param('data');
    my $data = $json->jsonToObj($cgi->param('data'));
    
    my $node = $data->{'node'};
    my $tree = $data->{'tree'};
    
    my $id = $node->{'objectId'};
    my $type = $node->{'widgetId'};
    my $tree_id = $tree->{'objectId'};

    detaint_natural($id);
    trick_taint($type);
    
    for ($type){
        /classification/ && do { get_products($id);               };
        /product/        && do { get_categories($id);             };
        /category/       && do { get_category_element_json($id)   };
        /element/        && do { get_element_children($id)        };
        /property/       && do { get_validexp_json($id,$tree_id)  };
        /environment/    && do { get_env_elements($id)            };
    }
}

elsif($action eq 'removeNode'){
    my $json = new JSON;
    my $data = $json->jsonToObj($cgi->param('data'));
    
    my $tree = $data->{'tree'};
    my $env_id = $tree->{'objectId'};
    
    my $node = $data->{'node'};
    my $id = $node->{'objectId'};
    my $type = $node->{'widgetId'};

    detaint_natural($env_id) unless $type =~ /validexp/;
    detaint_natural($id);
    trick_taint($type);
    
    my $env = Bugzilla::Testopia::Environment->new($env_id);
    $env->delete_element($id);
    
    print "true";
    
}

elsif($action eq 'set_selected'){
    my $type = $cgi->param('type');

    if ($type =~ /exp/)
    {
        my $env_id = $cgi->param('env_id');
        my $value = $cgi->param('value');
        $value =~ s/<span style='color:blue'>|<\/span>//g;
        $type =~ /^validexp(\d+)/;
        my $prop_id = $1; 

        detaint_natural($env_id);
        detaint_natural($prop_id);
        trick_taint($value);
        
        my $env = Bugzilla::Testopia::Environment->new($env_id);
        
        my $property = Bugzilla::Testopia::Environment::Property->new($prop_id);
        my $elmnt_id = $property->element_id();
        my $old = $env->get_value_selected($env->id,$elmnt_id,$property->id);
        $old = undef if $old eq $value;
        if ($env->store_property_value($prop_id,$elmnt_id,$value) == 0){
            $env->update_property_value($prop_id,$elmnt_id,$value);
        }
        my $json ='{message:"Updated selection for '. $property->name .'."';
        $json .= ',old: "validexp'. $prop_id. '~'. $old if $old;
        $json .= '"}';
        print $json; 
    }
}

elsif($action eq 'move'){
    my $json = new JSON;
    my $data = $json->jsonToObj($cgi->param('data'));
    
    my $element = $data->{'child'};
    my $env_tree = $data->{'newParentTree'};
    
    my $element_id = $element->{'objectId'};
    my $environment_id = $env_tree->{'objectId'};
    trick_taint($element_id);
    trick_taint($environment_id);
    
    my $env = Bugzilla::Testopia::Environment->new($environment_id);
    $element = Bugzilla::Testopia::Environment::Element->new($element_id);
    my $properties = $element->get_properties;
    if (scalar @$properties == 0){
        my $success = $env->store_property_value(0, $element_id, "");
    }
    foreach my $property (@$properties){
        my $success = $env->store_property_value($property->{'property_id'}, $element_id, "");
        if ($success == 0){print "{error:\"error\"";exit;}
    }
    
    print "true";exit;
    
    print "{error:\"";
    print "element_id: ".$element_id."<br>";
    print "environment_id: ".$environment_id."\"}";
}

else { 
    display();
}


sub display {
	detaint_natural($env_id);
    my $env = Bugzilla::Testopia::Environment->new($env_id);
    ThrowUserError('testopia-permission-denied', {object => 'Environment'}) unless $env->canedit;
    
    if(!defined($env)){
    	my $env = Bugzilla::Testopia::Environment->new({'environment_id' => 0});
	    ThrowUserError("testopia-read-only", {'object' => 'Environment'}) unless $env->canedit;
	    $vars->{'environment'} = $env;
	    $vars->{'action'} = 'do_add';
	    $template->process("testopia/environment/add.html.tmpl", $vars)
	        || print $template->error();
	        exit;
    }
    
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
    $vars->{'user'} = Bugzilla->user;
    $vars->{'action'} = 'do_edit';
    $vars->{'environment'} = $env;
    $template->process("testopia/environment/show.html.tmpl", $vars)
        || print $template->error();
        
}

###########################
### Tree Helper Methods ###
###########################
sub get_products{
    my ($class_id) = (@_);
    my $class = Bugzilla::Testopia::Classification->new($class_id);
    return unless scalar(grep {$class->id eq $class_id} @{Bugzilla->user->get_selectable_classifications});
    print $class->products_to_json(1);
}

sub get_categories{
    my ($product_id) = (@_);
    my $product = Bugzilla::Testopia::Product->new($product_id);
    return unless Bugzilla->user->can_see_product($product->name);
    my $category = Bugzilla::Testopia::Environment::Category->new({});
    print $category->product_categories_to_json($product_id,1);
}

sub get_category_element_json {
    my ($id) = (@_);
    my $category = Bugzilla::Testopia::Environment::Category->new($id);
    return unless $category->canedit;
    my $fish = $category->elements_to_json("TRUE");
    print $fish;
} 

sub get_element_children {
    my ($id) = (@_);
    my $element = Bugzilla::Testopia::Environment::Element->new($id);
    return unless $element->canedit;
    print $element->children_to_json(1);
}

sub get_env_elements {
    my ($id) = (@_);
    my $env = Bugzilla::Testopia::Environment->new($id);
    return unless $env->canedit;
    print $env->elements_to_json(1);
}

sub get_validexp_json {
    my ($id,$env_id) = (@_);
    my $property = Bugzilla::Testopia::Environment::Property->new($id);
    return unless $property->canedit;
    print $property->valid_exp_to_json(1,$env_id);
}
