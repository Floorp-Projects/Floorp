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

Bugzilla::Testopia::Environment::Property - A test environment element property

=head1 DESCRIPTION

Element properties describe the elements in more detail. An
element can have an unlimited number of properties to describe it.
The valid expression limits the possible descriptions to valid choices
for each property. 

=head1 SYNOPSIS

 $prop = Bugzilla::Testopia::Environment::Property->new($prop_id);
 $prop = Bugzilla::Testopia::Environment::Property->new(\%prop_hash);

=cut

package Bugzilla::Testopia::Environment::Property;

use strict;

use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::User;
use Bugzilla::Testopia::Environment::Element;

###############################
####    Initialization     ####
###############################

=head1 FIELDS

    property_id
    element_id
    name
    validexp

=cut

use constant DB_COLUMNS => qw(
    property_id
    element_id
    name
    validexp
    );

our $columns = join(", ", DB_COLUMNS);

###############################
####       Methods         ####
###############################

=head1 METHODS

=head2 new

Instantiates a new Property object

=cut

sub new {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
    my $self = {};
    bless($self, $class);
    return $self->_init(@_);
}

=head2 _init

Private constructor for the property class

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
              FROM test_environment_property
              WHERE property_id  = ?}, undef, $id);
    } elsif (ref $param eq 'HASH'){
         $obj = $param;   
    }

    return undef unless (defined $obj);

    foreach my $field (keys %$obj) {
        $self->{$field} = $obj->{$field};
    }
    return $self;
}


=head2 check_property

Returns id if a property exists

=cut

sub check_property{
    my $self = shift;
    my ($name, $element_id) = @_;
    my $dbh = Bugzilla->dbh;

    unless ($name && $element_id) {
        return "check_product must be passed a valid name and product_id";
    }

    my ($used) = $dbh->selectrow_array(qq{
    	SELECT property_id 
		  FROM test_environment_property
		  WHERE name = ? AND element_id = ?},undef,$name,$element_id);

    return $used;             
}

=head2 check_for_validexp

Returns 1 if a validexp exist for the property

=cut

sub check_for_validexp{
    my $self = shift;
    my $name = (@_);
    my $dbh = Bugzilla->dbh;

    my ($used) = $dbh->selectrow_array(qq{
    	SELECT 1 
		  FROM test_environment_property
		  WHERE property_id = ? AND (validexp IS NOT NULL AND validexp <> '')},undef,$self->{'property_id'});

    return $used;             
}

=head2 new_property_count

Returns the count + 1 of new properties

=cut

sub new_property_count{
    my $self = shift;
    my ($element_id) = @_;
    $element_id ||= $self->{'element_id'};
    my $dbh = Bugzilla->dbh;
    
    my ($used) = $dbh->selectrow_array(
       "SELECT COUNT(*) 
          FROM test_environment_property 
         WHERE name like 'New property%'
         AND element_id = ?",
         undef, $element_id);

    return $used + 1;             
}

=head2 get_validexp

Returns the validexp for the property

=cut

sub get_validexp{
    my $self = shift;
    my $name = (@_);
    my $dbh = Bugzilla->dbh;

    my $validexp = $dbh->selectrow_arrayref(qq{
    	SELECT validexp 
		  FROM test_environment_property
		  WHERE property_id = ? },undef,$self->{'property_id'});

    return $validexp;     
}


=head2 store

Serializes the new property to the database

=cut

sub store {
    my $self = shift;
    my $timestamp = Bugzilla::Testopia::Util::get_time_stamp();
 
    # Verify name is available
    return undef if $self->check_property($self->{'name'}, $self->{'element_id'});
	
    my $dbh = Bugzilla->dbh;
    $dbh->do("INSERT INTO test_environment_property ($columns)
             VALUES (?,?,?,?)",undef, (undef, $self->{'element_id'}, $self->{'name'}, $self->{'validexp'}));          
    my $key = $dbh->bz_last_key( 'test_plans', 'plan_id' );
    return $key;
}

=head2 set_name

Updates the property name in the database

=cut

sub set_name {
	my $self = shift;
    my $timestamp = Bugzilla::Testopia::Util::get_time_stamp();
    
    my ($name) = (@_);
    
    my $dbh = Bugzilla->dbh;
    $dbh->do("UPDATE test_environment_property 
              SET name = ? WHERE property_id = ?",undef, ($name,$self->{'property_id'} ));          
    return 1;
}


=head2 set_element

Updates the elmnt_id in the database

=cut

sub set_element {
	my $self = shift;
    my $timestamp = Bugzilla::Testopia::Util::get_time_stamp();
    
    my ($id) = (@_);
    
    my $dbh = Bugzilla->dbh;
    $dbh->do("UPDATE test_environment_property 
              SET element_id = ? WHERE property_id = ?",undef, ($id,$self->{'property_id'} ));          
    return 1;
}

=head2 update_property_validexp

Updates the property valid expression in the database

=cut

sub update_property_validexp {
    my $timestamp = Bugzilla::Testopia::Util::get_time_stamp();
    my $self = shift;
    my ($validexp) = (@_);
    
    my $dbh = Bugzilla->dbh;
    $dbh->do("UPDATE test_environment_property 
              SET validexp = ? WHERE property_id = ?",undef, ($validexp,$self->{'property_id'} ));          
    return 1;
}

sub valid_exp_to_json {
    my $self = shift;
    my ($disable_move, $env_id) = @_;
    my $env = Bugzilla::Testopia::Environment->new($env_id) if $env_id;
    
    $disable_move = $disable_move ? ',"addChild","move","remove"' : '';
    my $validexp = $self->get_validexp;
   
    my @validexpressions = split(/\|/, @$validexp[0]);
    
    my $json = '[';
    
    foreach (@validexpressions)
    {
        $json .= '{title: "' . $_ . '",';
        $json .=  'widgetId:"validexp' . $self->id . '~'. $_ .'",';
        $json .=  'actionsDisabled:["addCategory","addElement","addProperty","addValue"'. $disable_move .'],';
        $json .=  'objectId:"' . $self->id . '~' . $_ . '",';
        if ($env && $env->get_value_selected($env->id, $self->element_id, $self->id) eq $_){
            $json .=  'childIconSrc:"testopia/img/selected_value.png"},';
        }
        else{
            $json .=  'childIconSrc:""},';
        }
    }
    chop $json;
    $json .= ']';
    
    return $json;
}

=head2 obliterate

Completely removes the element property entry from the database.

=cut

sub obliterate {
    my $self = shift;
    my $dbh = Bugzilla->dbh;
	
    $dbh->do("DELETE FROM test_environment_map
               WHERE property_id = ?", undef, $self->id);
    $dbh->do("DELETE FROM test_environment_property 
              WHERE property_id = ?", undef, $self->id);
    
    return 1;
    
}

sub canedit {
    my $self = shift;
    my $element = Bugzilla::Testopia::Environment::Element->new($self->element_id);
    return 1 if $element->canedit;
    return 0;
}

sub candelete {
    my $self = shift;
    return 0 unless $self->canedit;
    my $dbh = Bugzilla->dbh;
    my $used = $dbh->selectrow_array("SELECT 1 FROM test_environment_map 
                                      WHERE property_id = ?",
                                      undef, $self->id);
    return !$used;

}

###############################
####      Accessors        ####
###############################
=head2 id

Returns the ID of this object

=head2 name

Returns the name of this object

=head2 validexp

Returns the valid expression associated with this property

=head2 element_id

Returns the element's id associated with this property


=cut

sub id              { return $_[0]->{'property_id'}; }
sub name            { return $_[0]->{'name'}; }
sub validexp        { return $_[0]->{'validexp'}; }
sub element_id      { return $_[0]->{'element_id'}; }

1;