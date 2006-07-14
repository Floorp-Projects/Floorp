# -*- mode: cperl; c-basic-offset: 8; indent-tabs-mode: nil; -*-

=head1 COPYRIGHT

 # ***** BEGIN LICENSE BLOCK *****
 # Version: MPL 1.1
 #
 # The contents of this file are subject to the Mozilla Public License
 # Version 1.1 (the "License"); you may not use this file except in
 # compliance with the License. You may obtain a copy of the License
 # at http://www.mozilla.org/MPL/
 #
 # Software distributed under the License is distributed on an "AS IS"
 # basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 # the License for the specific language governing rights and
 # limitations under the License.
 #
 # The Original Code is Litmus.
 #
 # The Initial Developer of the Original Code is
 # the Mozilla Corporation.
 # Portions created by the Initial Developer are Copyright (C) 2006
 # the Initial Developer. All Rights Reserved.
 #
 # Contributor(s):
 #   Chris Cooper <ccooper@deadsquid.com>
 #   Zach Lipton <zach@zachlipton.com>
 #
 # ***** END LICENSE BLOCK *****

=cut

package Litmus::DB::Product;

use strict;
use base 'Litmus::DBI';

Litmus::DB::Product->table('products');

Litmus::DB::Product->columns(All => qw/product_id name iconpath enabled/);
Litmus::DB::Product->columns(Essential => qw/product_id name iconpath enabled/);

Litmus::DB::Product->column_alias("product_id", "productid");

Litmus::DB::Product->has_many(testcases => "Litmus::DB::Testcase", 
                              { order_by => 'testcase_id' });
Litmus::DB::Product->has_many(subgroups => "Litmus::DB::Subgroup",
                              { order_by => 'name' });
Litmus::DB::Product->has_many(testgroups => "Litmus::DB::Testgroup",
                              { order_by => 'name' });
Litmus::DB::Product->has_many(branches => "Litmus::DB::Branch",
                              { order_by => 'name' });

__PACKAGE__->set_sql(ByPlatform => qq{
                                      SELECT pr.* 
                                      FROM products pr, platform_products plpr 
                                      WHERE plpr.platform_id=? AND plpr.product_id=pr.product_id
                                      ORDER BY pr.name ASC
});

#########################################################################
sub delete_from_platforms() {
  my $self = shift;
  
  my $dbh = __PACKAGE__->db_Main();
  my $sql = "DELETE from platform_products WHERE product_id=?";
  my $rows = $dbh->do($sql,
                      undef,
                      $self->product_id
                     );
}

#########################################################################
sub delete_with_refs() {
  my $self = shift;
  $self->delete_from_platforms();

  my $dbh = __PACKAGE__->db_Main();
  my $sql = "UPDATE testgroups SET product_id=0,enabled=0 WHERE product_id=?";
  my $rows = $dbh->do($sql,
                      undef,
                      $self->product_id
                     );

  # Remove references to product in other entities. 
  # Disable those entities for good measure.
  $sql = "UPDATE subgroups SET product_id=0,enabled=0 WHERE product_id=?";
  $rows = $dbh->do($sql,
                   undef,
                   $self->product_id
                  );

  $sql = "UPDATE testcases SET product_id=0,enabled=0,community_enabled=0 WHERE product_id=?";
  $rows = $dbh->do($sql,
                   undef,
                   $self->product_id
                  );

  $sql = "UPDATE branches SET product_id=0,enabled=0 WHERE product_id=?";
  $rows = $dbh->do($sql,
                   undef,
                   $self->product_id
                  );

  return $self->delete;
}

1;
