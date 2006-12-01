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

package Bugzilla::WebService::Testopia::Product;

use strict;

use base qw(Bugzilla::WebService);

use Bugzilla::Testopia::Product;

sub lookup_name_by_id
{
  my $self = shift;
  my ($product_id) = @_;
  
  die "Invalid Product ID" 
      unless defined $product_id && length($product_id) > 0 && $product_id > 0;
      
  $self->login;
  
  my $product = new Bugzilla::Testopia::Product($product_id);

  my $result = defined $product ? $product->name : '';
  
  $self->logout;
  
  # Result is product name string or empty string if failed
  return $result;
}

sub lookup_id_by_name
{
  my $self = shift;
  my ($name) = @_;

  $self->login;

  my $result = Bugzilla::Testopia::Product->check_product_by_name($name);
  
  $self->logout;

  if (!defined $result)
  {
    $result = 0;
  }

  # Result is product id or 0 if failed
  return $result;
}

#sub get_product 
#{
#    my $self = shift;
#    my ($product_id) = @_;
#
#    Bugzilla->login;
#
#    # We can detaint immediately if what we get passed is fully numeric.
#    # We leave bug alias checks to Bugzilla::Testopia::TestPlan::new.
#    
#    if ($product_id =~ /^[0-9]+$/) {
#        detaint_natural($product_id);
#    }
#
#    return new Bugzilla::Product($product_id);
#}

1;