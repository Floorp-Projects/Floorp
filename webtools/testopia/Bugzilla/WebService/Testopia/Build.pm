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
# The Original Code is the Bugzilla Bug Tracking System.
#
# Contributor(s): Marc Schumann <wurblzap@gmail.com>
#                 Dallas Harken <dharken@novell.com>

package Bugzilla::WebService::Testopia::Build;

use strict;

use base qw(Bugzilla::WebService);

use Bugzilla::Testopia::Build;

sub get
{
    my $self = shift;
    my ($build_id) = @_;

    $self->login;    

	#Result is a test plan hash map
    my $build = new Bugzilla::Testopia::Build($build_id);

	if (not defined $build)
	{
    	$self->logout;
        die "Build, " . $build_id . ", not found"; 
	}
	
    $self->logout;

    return $build;
}

sub create
{
	my $self = shift;
	my ($new_values) = @_;  # Required: name, product_id

    $self->login;

	my $build = new Bugzilla::Testopia::Build($new_values);
	
	my $name = $$new_values{name};
	
    if (defined($name) && $build->check_name($name))
    {
        die "Build name, " . $name . ", already exists"; 
    }

	my $result = $build->store(); 
	
	$self->logout;
	
	# Result is new build id
	return $result;
}

sub update
{
	my $self =shift;
	my ($build_id, $new_values) = @_;  # Modifiable: name, description, milestone

    $self->login;

	my $build = new Bugzilla::Testopia::Build($build_id);
	
	if (not defined $build)
	{
    	$self->logout;
        die "Build, " . $build_id . ", not found"; 
	}
	
	my $name = $$new_values{name};
	
    if (defined($name) && $build->check_name($name))
    {
        die "Build name, " . $name . ", already exists"; 
    }
    
    if (!defined($name))
    {
        $name = $build->name();
    }
    
    my $description = (defined($$new_values{description}) ? $$new_values{description} : $build->description()); 

    my $milestone = (defined($$new_values{milestone}) ? $$new_values{milestone} : $build->milestone()); 
    
    my $result = $build->update($name,
                                $description,
                                $milestone);

	$build = new Bugzilla::Testopia::Build($build_id);
	
	$self->logout;

	# Result is modified build, otherwise an exception will be thrown
	return $build;
}

sub lookup_name_by_id
{
  my $self = shift;
  my ($build_id) = @_;
  
  die "Invalid Build ID" 
      unless defined $build_id && length($build_id) > 0 && $build_id > 0;
      
  $self->login;
  
  my $build = new Bugzilla::Testopia::Build($build_id);

  my $result = defined $build ? $build->name : '';
  
  $self->logout;
  
  # Result is build name string or empty string if failed
  return $result;
}

sub lookup_id_by_name
{
  my $self = shift;
  my ($name) = @_;

  $self->login;

  my $result = Bugzilla::Testopia::Build->check_build_by_name($name);
  
  $self->logout;

  if (!defined $result)
  {
    $result = 0;
  }

  # Result is build id or 0 if failed
  return $result;
}

1;