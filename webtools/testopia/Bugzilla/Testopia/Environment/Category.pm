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
#                 Michael Hight <mjhight@gmail.com>
#                 Garrett Braden <gbraden@novell.com>

=head1 NAME

Bugzilla::Testopia::Environment::Category - A test element category

=head1 DESCRIPTION

Categories are used to organize environment elements. 

=head1 SYNOPSIS

 $prop = Bugzilla::Testopia::Environment::Category->new($env_category_id);
 $prop = Bugzilla::Testopia::Environment::Category->new(\%cat_hash);

=cut

package Bugzilla::Testopia::Environment::Category;

use strict;

use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Config;
use Bugzilla::User;
use Bugzilla::Constants;
use Bugzilla::Testopia::Product;

###############################
####    Initialization     ####
###############################

=head1 FIELDS

    env_category_id
    product_id
    name
    
=cut

use constant DB_COLUMNS => qw(
    env_category_id
    product_id
    name
    );

our $columns = join(", ", DB_COLUMNS);

###############################
####       Methods         ####
###############################

=head1 METHODS

=head2 new

Instantiates a new Category object

=cut

sub new {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
    my $self = {};
    bless($self, $class);
    return $self->_init(@_);
}

=head2 _init

Private constructor for the category class

=cut

sub _init {
    my $self = shift;
    my ($param) = (@_);
    my $dbh = Bugzilla->dbh;
	
    my $id = $param unless (ref $param eq 'HASH');
    my $obj;

    if (defined $id && detaint_natural($id)) {

        $obj = $dbh->selectrow_hashref(qq{
            SELECT $columns 
              FROM test_environment_category
              WHERE env_category_id  = ?}, undef, $id);
    } elsif (ref $param eq 'HASH'){
         $obj = $param;   
    }

    return undef unless (defined $obj);

    foreach my $field (keys %$obj) {
        $self->{$field} = $obj->{$field};
    }
    return $self;
}


=head2 get element list by category

Returns an array of element objects for a category

=cut

sub get_elements_by_category{
	my $self = shift;
	
    my $dbh = Bugzilla->dbh;
    
    my $ref = $dbh->selectcol_arrayref(qq{
    		SELECT element_id 
              FROM test_environment_element 
			  WHERE env_category_id = ?}, undef, $self->{'env_category_id'});
	
	my @elements;

	foreach my $val  (@$ref){
		push @elements, Bugzilla::Testopia::Environment::Element->new($val);
	};
	         
   return \@elements;             
}

=head2 get_parent_elements

Returns an array of parent elements by category

=cut

sub get_parent_elements{
	my $self = shift;
	
    my $dbh = Bugzilla->dbh;
    
################## original 
#                Matches null for parent id
#
#    my $ref = $dbh->selectcol_arrayref(qq{
#    		SELECT element_id 
#              FROM test_environment_element 
#              WHERE env_category_id = ? AND parent_id is null }, undef, $self->{'env_category_id'});
#
############### end original delete when ready


################## edited original
#                 Matches 0 for parent id
#
#    my $ref = $dbh->selectcol_arrayref(qq{
#    		SELECT element_id 
#              FROM test_environment_element 
#              WHERE env_category_id = ? AND parent_id = 0 }, undef, $self->{'env_category_id'});
#
############### end edited original keep this one when ready


########### temp fix to address issues with 0 or null in test_envrionment_element parent_id column
#####        matching null OR 0	
	 my $ref = $dbh->selectcol_arrayref(qq{
    		SELECT element_id 
              FROM test_environment_element 
              WHERE env_category_id = ? AND (parent_id is null or parent_id = 0) }, undef, $self->{'env_category_id'});
########### end temp fix.... delete when ready	

	my @elements;

	foreach my $val  (@$ref){
		push @elements, Bugzilla::Testopia::Environment::Element->new($val);
	};
	         
   return \@elements;             
}

=head2 check_for_elements

Returns 1 if a category has any elements

=cut

sub check_for_elements{
	my $self = shift;
	my $dbh = Bugzilla->dbh;
    
    my $ref = $dbh->selectrow_array(qq{
    		SELECT 1 
              FROM test_environment_element 
			  WHERE env_category_id = ?}, undef, $self->{'env_category_id'});
		         
   return $ref;             
}

=head2 get_product_list

Returns the product_id, product name, and count of categories

=cut

sub get_env_product_list{
	my $self = shift;
	my ($class_id) = @_;
    
    my $dbh = Bugzilla->dbh;
    my $query = "SELECT p.id, p.name, COUNT(tec.env_category_id) AS cat_count 
          		   FROM products p
          		   LEFT JOIN group_control_map
          		     ON group_control_map.product_id = p.id ";
    
    if (Param('useentrygroupdefault')) {
       $query .= "AND group_control_map.entry != 0 ";
    } else {
       $query .= "AND group_control_map.membercontrol = " .
              CONTROLMAPMANDATORY . " ";
    }
    if (%{Bugzilla->user->groups}) {
       $query .= "AND group_id NOT IN(" . 
              join(',', values(%{Bugzilla->user->groups})) . ") ";
    }
          		      
    $query .=  "LEFT OUTER JOIN test_environment_category AS tec
                  ON p.id = tec.product_id ";
    $query .= "WHERE group_id IS NULL ";
    $query .= "AND classification_id = ? " if $class_id;  
    $query .= "GROUP BY p.id
               ORDER BY p.name";
    
    
    my $ref;
    if($class_id){
        $ref = $dbh->selectall_arrayref($query, {'Slice' => {}}, $class_id);
    }
    else{
        $ref = $dbh->selectall_arrayref($query, {'Slice' => {}});
    }
    unshift @$ref, {'id' => 0, 'name' => '--ALL--', 'cat_count' => $self->get_all_child_count };
    return $ref;
                
}

sub get_all_child_count {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my ($all_count) = $dbh->selectrow_array(
        "SELECT COUNT(*) 
           FROM test_environment_category 
          WHERE product_id = 0");
          
	return $all_count;
}

sub product_categories_to_json {
    my $self = shift;
    my ($product_id, $disable_move) = @_;
    detaint_natural($product_id);
    $disable_move = ',"addChild","move"' if $disable_move;
    my $json = "[";
    foreach my $cat (@{$self->get_element_categories_by_product($product_id)}){
        $json .= '{title:"' . $cat->name . '",';
        $json .=  'isFolder:' . ($cat->check_for_elements() ? "true" : "false") . ',';
        $json .=  'objectId:"' . $cat->id . '",';
        $json .=  'widgetId:"category' . $cat->id . '",';
        $json .=  'actionsDisabled:["addCategory","addProperty","addValue"';
        $json .=  $disable_move if $disable_move;
        $json .=  ',"remove"' unless $cat->candelete;
        $json .=  '],';
        $json .=  'childIconSrc:"testopia/img/square.gif"},';
    }
    chop $json;
    $json .= "]";
    return $json;   
}

=head2 get_element_categories_by_product

Returns the list of element category names and ids by product id

=cut

sub get_element_categories_by_product{
    my $self = shift;
    my $dbh = Bugzilla->dbh;
    my ($product_id) = (@_);
    
    my $ref = $dbh->selectcol_arrayref(
                   "SELECT env_category_id 
                      FROM test_environment_category 
                     WHERE product_id = ?",
                    undef, $product_id);
    my @objs;
    foreach my $id (@{$ref}){
        push @objs, Bugzilla::Testopia::Environment::Category->new($id);
    }
    return \@objs;            
}


=head2 new_category_count

Returns 1 if element has children

=cut

sub new_category_count{
    my $self = shift;
    my ($prod_id) = @_;
    $prod_id ||= $self->{'product_id'};
    my $dbh = Bugzilla->dbh;
    
    my ($used) = $dbh->selectrow_array(
       "SELECT COUNT(*) 
          FROM test_environment_category 
         WHERE name like 'New category%'
         AND product_id = ?",
         undef, $prod_id);

    return $used + 1;             
}

sub elements_to_json {
    my $self = shift;
    my ($disable_add) = @_;
    $disable_add = ',"addChild"' if $disable_add;

    my $elements = $self->get_parent_elements;
    my $json = '[';
    
    foreach my $element (@$elements)
    {
        $json .= '{title:"'. $element->{'name'} .'",';
        $json .=  'objectId:"'. $element->{'element_id'}. '",';
        $json .=  'widgetId:"element'. $element->{'element_id'} .'",';
        $json .=  'actionsDisabled:["addCategory","addValue"';
        $json .=  $disable_add if $disable_add;
        $json .=  ',"remove"' unless $element->candelete;
        $json .=  '],';
        $json .=  'isFolder:true,' if($element->check_for_children || $element->check_for_properties);
        $json .=  'childIconSrc:"testopia/img/circle.gif"},'; 
    }
    chop $json;
    $json .= ']';

    return $json;   
}
    
=head2 check_category

Returns category id if a category exists

=cut

sub check_category{
    my $dbh = Bugzilla->dbh;
    my $self = shift;
	my ($name, $prodID) = (@_);
	
	$prodID ||= $self->product_id;
	
    my ($used) = $dbh->selectrow_array(
       "SELECT env_category_id 
		  FROM test_environment_category
	     WHERE name = ? AND product_id = ?",
	     undef,($name,$prodID));

    return $used;             
}

=head2 check_category_by_id

Returns category name if a category id exists

=cut

sub check_category_by_id{
    my $dbh = Bugzilla->dbh;
	my $self = shift;
    my ($id) = (@_);

	my ($used) = $dbh->selectrow_arrayref(qq{
    	SELECT name 
		  FROM test_environment_category
		  WHERE env_category_id = ?},undef,$id);

    return $used;             
}


=head2 store

Serializes this category to the database and returns the key or 0 

=cut

sub store {
	my $self = shift;
    my $timestamp = Bugzilla::Testopia::Util::get_time_stamp();
    
    return 0 if $self->check_category($self->{'name'},$self->{'product_id'});
	
    my $dbh = Bugzilla->dbh;
    $dbh->do("INSERT INTO test_environment_category ($columns)
              VALUES (?,?,?)",undef, (undef, $self->{'product_id'},$self->{'name'}));          
    my $key = $dbh->bz_last_key( 'test_environment_category', 'env_category_id' );

    return $key;
}

=head2 set_name

Updates the category name in the database

=cut

sub set_name {
    my $self = shift;
    my ($name) = (@_);
    my $dbh = Bugzilla->dbh;
    
    return undef if $self->check_category($name);
    
    $dbh->do("UPDATE test_environment_category SET name = ? 
              WHERE env_category_id = ? AND product_id = ?",
              undef, ($name, $self->{'env_category_id'},$self->{'product_id'}));          
    return 1;
}

=head2 set_product

Updates the category in the database

=cut

sub set_product {
    my $self = shift;
    my ($product_id) = (@_);
    
    return if ($product_id == $self->{'product_id'});  
    
    my $dbh = Bugzilla->dbh;
    $dbh->do("UPDATE test_environment_category SET product_id = ? 
              WHERE env_category_id = ? AND product_id = ?",
              undef, ($product_id, $self->{'env_category_id'},$self->{'product_id'}));          
    return 1;
}

=head2 obliterate

Completely removes the element category entry from the database.

=cut

sub obliterate {
    my $self = shift;
	my $dbh = Bugzilla->dbh;
	
    $dbh->bz_lock_tables('test_environment_element AS tee READ', 'test_environment_map AS tem READ', 'test_environment_category WRITE');

    if (!$self->candelete) {
        $dbh->bz_unlock_tables;
        return 0;
    }
    $dbh->do("DELETE FROM test_environment_category 
              WHERE env_category_id = ?", undef, $self->id);
    $dbh->bz_unlock_tables;
    
    return 1;
    
}

sub canedit {
    my $self = shift;
    return 1 if Bugzilla->user->in_group('Testers');
    my $product = Bugzilla::Testopia::Product->new($self->product_id);
    return 1 if Bugzilla->user->can_see_product($product->name);
    return 0;
}

sub candelete {
    my $self = shift;
    return 0 unless $self->canedit;
    my $dbh = Bugzilla->dbh;
    my $used = $dbh->selectrow_array(
        "SELECT 1 FROM test_environment_map AS tem
           JOIN test_environment_element AS tee ON tee.element_id = tem.element_id   
          WHERE tee.env_category_id = ?",
          undef, $self->id);
    return !$used;

}

###############################
####      Accessors        ####
###############################
=head2 id

Returns the ID of this category

=head2 name

Returns the name of this category

=head2 product_id

Returns the product_id of this category

=cut

sub id              { return $_[0]->{'env_category_id'}; }
sub name            { return $_[0]->{'name'}; }
sub product_id      { return $_[0]->{'product_id'}; }


=head2 product_name

Returns the name of the product this plan is associated with

=cut

sub product_name {
    my ($self) = @_;
    my $dbh = Bugzilla->dbh;
    return $self->{'product_name'} if exists $self->{'product_name'};

    my ($name) = $dbh->selectrow_array(
            "SELECT name FROM products
              WHERE id = ?", 
             undef, $self->id);
    $self->{'product_name'} = $name;
    return $self->{'product_name'};
}

1;
