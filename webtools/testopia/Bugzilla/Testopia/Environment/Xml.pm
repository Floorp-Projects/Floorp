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
# Contributor(s): Garrett Braden <gbraden@novell.com>


=head1 NAME

Bugzilla::Testopia::Environment::Xml - An XML representation of the Environment Object.

=head1 DESCRIPTION

This module is used to import and export environments via XML.  It can parse an XML representation
of a Testopia Environment and persist it to the database.  An Environment.pm XML object can be
initialized using new and passing it an XML scalar.  It can also take two other parameters:
    
    $admin - a boolean that automatically will store the imported environment to the database.
    $max_depth - the max depth of child elements to import.
    
Example:
    my $env_xml = Bugzilla::Testopia::Environment::Xml->new($xml, 1, 5);
    
Other subroutines can be called on the object.  For example:
    parse - takes the same three parameters as new
    store - stores the imported xml Environment object to the database
    
Misc. Other:
    the module also contains two other valueable fields on it's hash:
        $self->{'message'} - running list of valueable information upon parsing and storing
        $self->{'error'} - running list of error messages upon parsing and storing
    One other method exists(check_new_items) to check if there are new Elements, Properties, Categories,
    and Selected Values and returns a scalar value report of the new items not present in the database.
    
Import XML Environment Implementation Example: see tr_import_environment.cgi
    
To export an environment by env_id to XML use export

Example:
    my $xml = Bugzilla::Testopia::Environment::Xml->export($env_id);
    
Export Environment XML Implementation Example: see tr_export_environment.cgi

=head1 SYNOPSIS

use Bugzilla::Testopia::Environment::Xml;

=cut

package Bugzilla::Testopia::Environment::Xml;

#************************************************** Uses ****************************************************#
use strict;
use warnings;
use CGI;
use lib ".";
use XML::Twig;
use Bugzilla::Util;
use Bugzilla::User;
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Error;
use Bugzilla::Product;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::Environment;
use Bugzilla::Testopia::Product;
use Bugzilla::Testopia::Environment::Category;
use Bugzilla::Testopia::Environment::Element;
use Bugzilla::Testopia::Environment::Property;

our constant $max_depth = 7;


=head2 new

Instantiates a new Bugzilla::Testopia::Environment::Xml object

=cut

sub new {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
    my $self = {};
    bless($self, $class);
    return $self->_init(@_);
}


=head2 _init

Private constructor for the Bugzilla::Testopia::Environment::XML class

=cut

sub _init {
    my ($self, $xml, $admin, $depth) = @_;
    if (ref $xml eq 'HASH') {
        $self = $xml;
    }
    elsif($xml) {
        parse($self, $xml, $admin, $depth);
    }
    return undef unless (defined $xml);
    return $self;
}


=head2 Parse Environment

=head2 DESCRIPTION

Parses the Environment XML

=cut

sub parse() {
    my ($self, $xml, $admin, $depth) = @_;
    
    if($depth eq undef) {
        $self->{'max_depth'} = $max_depth;
    }
    else {
        $self->{'max_depth'} = $depth
    }
    if ($admin) {
        $self->{'message'} = "Importing XML Environment...<BR />";
    }
    else {
        $self->{'message'} = "Parsing and Validating XML Environment...<BR />";
    }
    $self->{'error'} = undef;
    if ($xml) {
       trick_taint($xml);
    }
    my $twig = XML::Twig->new();
    $twig->parse($xml);
    my $root = $twig->root;
    # Checking if Product and Environment already exist.
    my $product_name = $root->{'att'}->{'product'};
    my $product_id;
    if (lc($product_name) eq "--all--") {
        $self->{'message'} .= "..Using the <U>--ANY PRODUCT--</U> <STRONG>PRODUCT</STRONG>.<BR />";
        $product_id = 0;
    }
    else {
        $self->{'message'} .= "..Checking if <U>$product_name</U> <STRONG>PRODUCT</STRONG> already exists...";
        ($product_id) = Bugzilla::Testopia::Product->check_product_by_name($product_name);
        if ($product_id) {
            $self->{'message'} .= "EXISTS.<BR />";
        }
        else {
            $self->{'message'} .= "<U><FONT COLOR='RED'><STRONG>DOESN'T EXIST</STRONG></FONT></U>.<BR />Importing XML Environment Failed!<BR/>";
            $self->{'error'} .= "<U>$product_name</U> <STRONG>PRODUCT</STRONG> doesn't exist.  Please be sure to use an existing product.<BR />";
            return 0;
        }
    }
    ($self->{'product_id'}) = $product_id;
    $self->{'product_name'} = $product_name;
    my $environment_name = $root->{'att'}->{'name'};
    $self->{'name'} = $environment_name;
    $self->{'message'} .= "..Checking if <U>$environment_name</U> <STRONG>ENVIRONMENT NAME</STRONG> already exists for the <U>$product_name</U> <STRONG>PRODUCT</STRONG>...";
    my $environment = Bugzilla::Testopia::Environment->new({});
    my ($env_id) = $environment->check_environment($environment_name, $product_id);
    my $environment_id;        
    if ($env_id < 1) {
        $self->{'message'} .= "<U><FONT COLOR='RED'><STRONG>DOESN'T EXIST</STRONG></FONT></U><BR />";
        # Storing New Environment if Admin
        if ($admin) {
                $self->{'message'} .= "....Storing new <U>$environment_name</U> <STRONG>ENVIRONMENT NAME</STRONG> in the <U>$self->{'product_name'}</U> <STRONG>PRODUCT</STRONG>...";
                $environment->{'name'} = $environment_name;
                ($environment_id) = Bugzilla::Testopia::Environment->store_environment_name($self->{'name'}, $product_id);
                $self->{'message'} .= "DONE.<BR />";
        }
    }
    else {
        ($environment_id) = $env_id;
        $self->{'message'} .= "EXISTS<BR />Importing XML Environment Failed!<BR/>";
        $self->{'error'} .= "<U>$environment_name</U> <STRONG>ENVIRONMENT NAME</STRONG> already exists for the <U>$product_name</U> <STRONG>PRODUCT</STRONG>.  Please use another name.";
        return 0;
    }
    ($self->{'environment_id'}) = $environment_id;
    ($environment->{'product_id'}) = $self->{'product_id'};
    # Parse recursively through the nested child elements.
    foreach my $twig_category ($root->children("category")) {
        my $category_name = $twig_category->{'att'}->{'name'};
        # Makes sure to get the category_id by name and product_id
        my $category = Bugzilla::Testopia::Environment::Category->new({});
        my ($cat_id) = $category->check_category($category_name, $product_id);
        my $category_id;
        # Checking if Categories already exist.
        $self->{'message'} .= "..Checking if <U>$category_name</U> <STRONG>CATEGORY</STRONG> already exists...";
        if ($cat_id < 1) {
            $self->{'message'} .= "<U><FONT COLOR='RED'><STRONG>DOESN'T EXIST</STRONG></FONT></U>.<BR />";
            my $new_category_names = $self->{'new_category_names'};
            push (@$new_category_names, $category_name);
            $self->{'new_category_names'} = $new_category_names;
            # Storing New Categories if Admin
            if ($admin) {
                $self->{'message'} .= "....Storing new <U>$category_name</U> <STRONG>CATEGORY</STRONG> in the <U>$self->{'product_name'}</U> <STRONG>PRODUCT</STRONG>...";
                ($category->{'product_id'}) = $product_id;
                $category->{'name'} = $category_name;
                ($category_id) = $category->store();
                $self->{'message'} .= "DONE.<BR />";
            }
        }
        else {
            ($category_id) = $cat_id;
            $self->{'message'} .= "EXISTS.<BR />";
        }
        foreach my $twig_element ($twig_category->children("element")) {
            my $element = $self->parse_child_elements(1, $category_id, $category_name, $twig_element, $admin);
            my $elements = $self->{'elements'};
            push (@$elements, $element);
            $self->{'elements'} = $elements; 
        }
    }
    if ($admin) {
        $self->{'message'} .= "Finished Importing XML Environment!<BR/>";
    }
    else {
        $self->{'message'} .= "Finished Parsing and Validating XML Environment!<BR/>";
    }
}


=head2 Parse Children Elements

=head2 DESCRIPTION

Parses through elements and their children elements recursively

=cut

sub parse_child_elements() {
    my ($self, $depth, $env_category_id, $category_name, $twig_element, $admin, $parent_element) = @_;
    if ($depth > $self->{'max_depth'}) {
        return;
    }
    $depth++;
    my $element_name = $twig_element->{'att'}->{'name'};
    # Checking if Elements already exist.
    for (my $i = 1; $i < $depth; $i++) {
       $self->{'message'} .= "....";
    }
    $self->{'message'} .= "Checking if <U>$element_name</U> <STRONG>ELEMENT</STRONG> already exists in the <U>$category_name</U> <STRONG>CATEGORY</STRONG>...";
    my ($product_id) = $self->{'product_id'};
    my $element = Bugzilla::Testopia::Environment::Element->new({});
    my ($elem_id) = $element->check_element($element_name, $env_category_id);
    my $element_id;    
    if ($elem_id < 1) {
        $self->{'message'} .= "<U><FONT COLOR='RED'><STRONG>DOESN'T EXIST</STRONG></FONT></U>.<BR />";
        my $new_category_elements = $self->{'new_category_elements'};
        my $new_category_element = {'env_category_id' => $env_category_id, 'category_name' => $category_name, 'element_name' => $element_name};
        push (@$new_category_elements, $new_category_element);
        $self->{'new_category_elements'} = $new_category_elements;
        # Storing New Elements if Admin
        if ($admin) {
              for (my $i = 1; $i < $depth; $i++) {
                $self->{'message'} .= "....";
            }
            $self->{'message'} .= "..Storing new <U>$element_name</U> <STRONG>ELEMENT</STRONG> in the <U>$category_name</U> <STRONG>CATEGORY</STRONG>...";
            ($element->{'env_category_id'}) = $env_category_id;
            $element->{'name'} = $element_name;
            ($element->{'product_id'}) = $self->{'product_id'};
            $element->{'isprivate'} = 0;
            if ($parent_element) {
                $element->{'parent_id'} = $parent_element->{'element_id'};
            }
            ($element_id) = $element->store();
            $self->{'message'} .= "DONE.<BR />";
        }
    }
    else {
        ($element_id) = $elem_id;
        $self->{'message'} .= "EXISTS.<BR />";
    }
    ($element->{'element_id'}) = $element_id;
    ($element->{'env_category_id'}) = $env_category_id;
    $element->{'name'} = $element_name;
    ($element->{'parent_id'}) = $parent_element->{'parent_id'};
    my @properties;
    foreach my $twig_property ($twig_element->children("property")) {
        my $property_name = $twig_property->{'att'}->{'name'};
        # Checking if Properties already exist.
        for (my $i = 1; $i < $depth; $i++) {
            $self->{'message'} .= "....";
        }
        $self->{'message'} .= "....Checking if <U>$property_name</U> <STRONG>PROPERTY</STRONG> already exists...";
        my $property = Bugzilla::Testopia::Environment::Property->new({});
        my ($prop_id) = $property->check_property($property_name, $element_id);
        my $property_id;
        if ($prop_id < 1) {
            $self->{'message'} .= "<U><FONT COLOR='RED'><STRONG>DOESN'T EXIST</STRONG></FONT></U>.<BR />";
            my $new_property_names = $self->{'new_property_names'};
            push (@$new_property_names, $property_name);
            $self->{'new_property_names'} = $new_property_names; 
            # Storing New Property if Admin
            if ($admin) {
                for (my $i = 1; $i < $depth; $i++) {
                    $self->{'message'} .= "....";
                }
                $self->{'message'} .= "......Storing new <U>$property_name</U> <STRONG>PROPERTY</STRONG>...";
                $property->{'name'} = $property_name;
                ($property->{'element_id'}) = $element_id;
                ($property_id) = $property->store();
                $self->{'message'} .= "DONE.<BR />";
            }
        }
        else {
            ($property_id) = $prop_id;
            $self->{'message'} .= "EXISTS.<BR />";
        }
        $property = Bugzilla::Testopia::Environment::Property->new($property_id);
        # Checking if new Selected Value and Valid Expression exist.
        my $validexp;
        if ($property) {
           $validexp = $property->validexp();
        }
        my $value = $twig_property->field('value');
        for (my $i = 1; $i < $depth; $i++) {
            $self->{'message'} .= "....";
        }
        $self->{'message'} .= "........Checking if <U>$value</U> <STRONG>VALUE</STRONG> exists in the list of selectable values...";
        if ( $validexp !~ m/$value/) {
            $self->{'message'} .= "<U><FONT COLOR='RED'><STRONG>DOESN'T EXIST</STRONG></FONT></U>.<BR/>";
            if ($admin) {
                if (!defined($validexp)) {
                    for (my $i = 1; $i < $depth; $i++) {
                        $self->{'message'} .= "....";
                    }
                    $self->{'message'} .= "..........Setting <U>$value</U> <STRONG>VALID EXPRESSION</STRONG> equal to the <STRONG>VALUE</STRONG> for the first time...";
                    $validexp = $value;
                }
                else {
                    for (my $i = 1; $i < $depth; $i++) {
                        $self->{'message'} .= "....";
                    }
                    $self->{'message'} .= "..........Adding <U>$value</U> <STRONG>VALUE</STRONG> to the <STRONG>VALID EXPRESSION</STRONG>...";
                    $validexp = "$validexp | $value";
                }
                $property->update_property_validexp($validexp);
                $self->{'message'} .= "DONE.<BR/>";                
            }
            else {
                my $new_validexp_values = $self->{'new_validexp_values'};
                my $new_validexp_value = {'property_id' => $property_id, 'property_name' => $property_name, 'value' => $value};
                push (@$new_validexp_values, $new_validexp_value);
                $self->{'new_validexp_values'} = $new_validexp_values;
            }
        }
        elsif (!defined($validexp)) {
           $self->{'message'} .= "<STRONG>VALID EXPRESSION</STRONG> <U><FONT COLOR='RED'><STRONG>DOESN'T EXIST</STRONG></FONT></U> YET.<BR/>";
        }
        else {
            $self->{'message'} .= "EXISTS.<BR/>";
        }
        if ($property_id && $admin) {
            for (my $i = 1; $i < $depth; $i++) {
                $self->{'message'} .= "....";
            }
            $self->{'message'} .= "............Storing new <STRONG>VALUE SELECTED</STRONG> <U>$value</U>...";
            my $environment = Bugzilla::Testopia::Environment->new($self->{'environment_id'});
            $environment->store_property_value($property_id, $element_id, $value);
            $self->{'message'} .= "DONE.<BR/>";
        }
        $property->{'value_selected'} = $value;
        push (@properties, $property);
    }
    my $elm_properties = $element->{'properties'};
    push (@$elm_properties, @properties);
    if ($parent_element) {
        my $children = $parent_element->{'children'};
        push (@$children, $element);
        $parent_element->{'children'} = $children;
    }
    foreach my $twig_element_child ($twig_element->children("element")) {
        $self->parse_child_elements($depth, $env_category_id, $category_name, $twig_element_child, $admin, $element); 
    }
    return $element;
}


=head2 Checking Exists

=head2 DESCRIPTION

Checking if the Environment, Elements, Categories, and Properties already exist or not.

=cut

sub check_new_items() {
    my $self = shift;
    my $report;
    my $new_category_names = $self->{'new_category_names'};
    foreach my $new_category_name (@$new_category_names) {
        $report .= "New <U>$new_category_name</U> <STRONG>CATEGORY</STRONG>.<BR/>";
    }
    my $new_category_elements = $self->{'new_category_elements'};
    foreach my $new_category_element (@$new_category_elements) {
        $report .= "New <U>$new_category_element->{'element_name'}</U> <STRONG>ELEMENT</STRONG> in the ";
        if (!$new_category_element->{'env_category_id'}){
            $report .= "<STRONG>new</STRONG> ";
        }
        $report .= "<U>$new_category_element->{'category_name'}</U> <STRONG>CATEGORY</STRONG>.<BR/>";
    }
    my $new_property_names = $self->{'new_property_names'};
    foreach my $new_property_name (@$new_property_names) {
        $report .= "New <U>$new_property_name</U> <STRONG>PROPERTY</STRONG>.<BR/>";
    }
    my $new_validexp_values = $self->{'new_validexp_values'};
    foreach my $new_validexp_value (@$new_validexp_values) {
        $report .= "New <U>$new_validexp_value->{'value'}</U> <STRONG>VALUE</STRONG> for the ";
        if (!$new_validexp_value->{'property_id'}){
            $report .= "<STRONG>new</STRONG> ";
        }
        $report .= "<U>$new_validexp_value->{'property_name'}</U> <STRONG>PROPERTY</STRONG>'s selectable value list.<BR/>";
    }
    return $report;
}


=head2 Store the Environment

=head2 Description

Store the Environment Name, Element-Category relationship, Element-Property relationship,
Element-ChildElement relationship, Environment-Element-Property-Value.

=cut

sub store() {
    my $self = shift;
    $self->{'message'} .= "Storing new XML Environment...";
    if (!$self->{'environment_id'}) {
        $self->{'environment_id'} = Bugzilla::Testopia::Environment->store_environment_name($self->{'name'}, $self->{'product_id'});
    }
    my $environment = Bugzilla::Testopia::Environment->new($self);
    my $success = $environment->update();
    if (!$success) {
        $self->{'message'} .= "ABORTED!<BR/>";
        $self->{'error'} .= "Failed to store the Environment!<BR/>";
        return 0;
    }
    $self->{'message'} .= "DONE!<BR/>";
    return 1;
}


=head2 export

=head2 Description

Exports and Environment by env_id to a scalar XML value

=cut

sub export() {
    my $self = shift;
    my ($env_id) = @_;
    
    my $xml;
    
    my $environment = Bugzilla::Testopia::Environment->new($env_id);
    
    $xml =
        "<?xml version='1.0' encoding='UTF-8'?>" .
        "<!DOCTYPE environment SYSTEM 'environment.dtd' >" .
        "<environment name='$environment->{'name'}' product='";

    my $product = Bugzilla::Product->new($environment->{'product_id'});
    
    $xml .= "$product->{'name'}'>";
    
    $environment->get_environment_elements();
    
    my $categories = {};
    my $category_names = [];
    my $categorized_elements = [];
    my $elements = $environment->{'elements'};
    my $used_elements = {};
    foreach my $element (@$elements) {
        my $root_element = $self->get_root_parent($element);
        my $category_name = $root_element->cat_name();
        $categorized_elements = $categories->{ $category_name };
        for my $used_element (@$categorized_elements) {
            $used_elements->{ $used_element->{'element_id'} } = 1;
        }
        if (!$used_elements->{$root_element->{'element_id'}}) {
            push (@$categorized_elements, $root_element);
            $categories->{ $category_name } = $categorized_elements;
        }
    }
    
    foreach my $category_name (keys %$categories) {
        $xml .= "<category name='$category_name'>";
        
        $elements = $categories->{$category_name};
        
        
        foreach my $element (@$elements) {
            $element->get_children();
            $xml .= $self->export_element_and_children(1, $element, $environment->{'environment_id'});
        }
        
        $xml .= "</category>";
    }

    return "$xml</environment>";
}


=head2 get_root_parent

=head2 Description

Helper sub that returns the root parent element in the environment of the passed in element

=cut

sub get_root_parent {
    my $self = shift;
    my ($element) = @_;
    my $parent = $element->get_parent(); 
    if ($parent->type eq 'env_category') {
        return $element;
    }
    else {
        $self->get_root_parent($parent);
    }
}


=head2 export_element_and_children

=head2 Description

Exports Elements and their child elements to XML Recursively.

=cut

sub export_element_and_children() {
    my $self = shift;
    my ($depth, $element, $env_id) = @_;
    if ($depth > $max_depth) {
        return;
    }
    $depth++;
    
    my $xml = "<element name='$element->{'name'}'>";
    
    my $properties = $element->{'properties'};
    foreach my $property (@$properties) {
        my $value_selected = Bugzilla::Testopia::Environment->get_value_selected(
            $env_id, $element->{'element_id'}, $property->{'property_id'});
        if (defined($value_selected)) {
            $xml .=
                "<property name='$property->{'name'}'>" .
                "<value>$value_selected</value></property>";
        }
    }
    
    my $children = $element->{'children'};
    foreach my $child_element (@$children) {
        $child_element->get_children();
        $xml .= $self->export_element_and_children($depth, $child_element, $env_id);
    }
    $xml .= "</element>";
    return $xml;
}

1;
