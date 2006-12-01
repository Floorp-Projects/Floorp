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

package Bugzilla::WebService::User;

use strict;

use Bugzilla;

use base qw(Bugzilla::WebService);

sub lookup_login_by_id
{
  my $self = shift;
  my ($author_id) = @_;

  $self->login;
  
  my $user = new Bugzilla::User($author_id);

  my $result = defined $user ? $user->login : '';
  
  $self->logout;
  
  # Result is user login string or empty string if failed
  return $result;
}

sub lookup_id_by_login
{
  my $self = shift;
  my ($author) = @_;

  $self->login;

  my $result = Bugzilla::User::login_to_id($author);
  
  $self->logout;
  
  # Result is user id or 0 if failed
  return $result;
}

1;