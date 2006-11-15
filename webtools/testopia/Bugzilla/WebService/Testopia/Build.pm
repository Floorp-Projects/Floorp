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

sub lookup_name_by_id
{
  my $self = shift;
  my ($build_id) = @_;
  
  die "Invalid Build ID" 
      unless defined $build_id && length($build_id) > 0 && $build_id > 0;
      
  Bugzilla->login;
  
  my $build = new Bugzilla::Testopia::Build($build_id);

  my $result = defined $build ? $build->name : '';
  
  Bugzilla->logout;
  
  # Result is build name string or empty string if failed
  return $result;
}

sub lookup_id_by_name
{
  my $self = shift;
  my ($name) = @_;

  Bugzilla->login;

  my $result = Bugzilla::Testopia::Build->check_build_by_name($name);
  
  Bugzilla->logout;

  if (!defined $result)
  {
    $result = 0;
  }

  # Result is build id or 0 if failed
  return $result;
}

1;