#!/usr/bin/perl
# -*- Mode: perl -*-
#======================================================================
# FILE: Component.pm
# CREATOR: eric 1 Mar 01
#
# DESCRIPTION:
#   
#
#  $Id: Component.pm,v 1.1 2001/12/21 19:21:18 mikep%oeone.com Exp $
#  $Locker:  $
#
# (C) COPYRIGHT 2000, Eric Busboom, eric@softwarestudio.org
#
# This package is free software and is provided "as is" without express
# or implied warranty.  It may be used, redistributed and/or modified
# under the same terms as perl itself. ( Either the Artistic License or the
# GPL. ) 
#
#
#======================================================================



package Net::ICal::Libical::Component;
use Net::ICal::Libical;

use strict;

sub new{
  my $class = shift;
  my $ical_str = shift; # Ical data in string form
  my $self = {};

  $self->{'comp_p'} = Net::ICal::Libical::icalparser_parse_string($ical_str);

  die "Can't parse string into component" if !$self->{'comp_p'};

  bless $self, $class;
}

sub new_from_ref {
  my $class = shift;
  my $r = shift;
  my $self = {};

  $self->{'comp_p'} = $r;

  bless $self, $class;
}

# Destroy must call icalcomponent_free() if icalcomponent_get_parent()
# returns NULL
sub DESTROY {
  my $self = shift;
  
  my $c = $self->{'comp_p'};
  
  if($c && !Net::ICal::Libical::icalcomponent_get_parent($c)){
    Net::ICal::Libical::icalcomponent_free($c);
  }
  
}

# Return an array of all properties of the given type
sub properties{

  my $self = shift;
  my $prop_name = shift;
  
  my @props;
  
  if(!$prop_name){
    $prop_name = 'ANY';
  }

  # To loop over properties
  # $comp_p = $self->{'comp_p'}
  # $p = icallangbind_get_first_property($comp_p,$prop_name)
  # $p = icallangbind_get_next_property($comp_p,$prop_name)

  my $c = $self->{'comp_p'};
  my $p; 

  for($p = Net::ICal::Libical::icallangbind_get_first_property($c,$prop_name);
     $p;
     $p = Net::ICal::Libical::icallangbind_get_next_property($c,$prop_name)){
    
    my $d_string = Net::ICal::Libical::icallangbind_property_eval_string($p,"=>");
    my %dict = %{eval($d_string)};
    
    $dict{'ref'} = $p;

  # Now, look at $dict{'value_type'} or $dict{'name'} to construct a 
  # derived class of Property. I'll do this later. 

    my $prop;

    if($dict{'value_type'} eq 'DATE' or $dict{'value_type'} eq 'DATE-TIME'){
      $prop = new Net::ICal::Libical::Time(\%dict);
    } elsif($dict{'value_type'} eq 'DURATION' ) {
      $prop = new Net::ICal::Libical::Duration(\%dict);
    } else  {
      $prop = new Net::ICal::Libical::Property(\%dict);
    }

    push(@props,$prop);

  }


  return @props;
  
}

  
sub add_property {
  
  # if there is a 'ref' key in the prop's dict, then it is owned by
  # an icalcomponent, so dont add it again. But, you may check that
  # it is owned by this component with:
  # icalproperty_get_parent(p->{'ref'}') != $self->{'comp_p'}
  
  # If there is no 'ref' key, then create one with $p->{'ref'} =
  # icalproperty_new_from_string($p->as_ical_string)
  
}

sub remove_property { 

# If $p->{'ref'} is set, then remove the property with
# icalcomponent_remove_property() }
}

# Return an array of all components of the given type
sub components {

  my $self = shift;
  my $comp_name = shift;
  
  my @comps;
  
  if(!$comp_name){
    $comp_name = 'ANY';
  }

  my $c = $self->{'comp_p'};
  my $p; 

  for($p = Net::ICal::Libical::icallangbind_get_first_component($c,$comp_name);
     $p;
     $p = Net::ICal::Libical::icallangbind_get_next_component($c,$comp_name)){
    
    push(@comps, Net::ICal::Libical::Component->new_from_ref($p));

  }

  return @comps;
  
}


sub add_component {}

sub remove_component {}

sub as_ical_string {
  my $self = shift;

  return Net::ICal::Libical::icalcomponent_as_ical_string($self->{'comp_p'})
}



1;
