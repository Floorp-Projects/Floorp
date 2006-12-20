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

package Bugzilla::WebService::Component;

use strict;

use Bugzilla;

use base qw(Bugzilla::WebService);

sub get
{
  my $self = shift;
  my ($component_id) = @_;

  $self->login;
  
  my $component = new Bugzilla::Component($component_id);

  $self->logout;
  
  # Result is component hash map on success
  return $component;
}

1;