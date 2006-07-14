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

package Litmus::DB::Platform;

use strict;
use base 'Litmus::DBI';
use CGI;

Litmus::DB::Platform->table('platforms');

Litmus::DB::Platform->columns(All => qw/platform_id name detect_regexp iconpath/);
Litmus::DB::Platform->columns(Essential => qw/platform_id name detect_regexp iconpath/);

Litmus::DB::Platform->column_alias("platform_id", "platformid");

Litmus::DB::Platform->has_many(opsyses => "Litmus::DB::Opsys");

Litmus::DB::Platform->set_sql(ByProduct => qq{
                                              SELECT pl.* 
                                              FROM platforms pl, platform_products plpr
                                              WHERE plpr.product_id=? AND plpr.platform_id=pl.platform_id
});

Litmus::DB::Platform->set_sql(ByProductAndName => qq{
                                                     SELECT pl.* 
                                                     FROM platforms pl, platform_products plpr
                                                     WHERE plpr.product_id=? AND plpr.platform_id=pl.platform_id
                                                     AND pl.name=?
});

#########################################################################
sub delete_from_products() {
  my $self = shift;
  
  my $dbh = __PACKAGE__->db_Main();
  my $sql = "DELETE from platform_products WHERE platform_id=?";
  my $rows = $dbh->do($sql,
                      undef,
                      $self->platform_id
                     );
}

#########################################################################
sub delete_with_refs() {
  my $self = shift;

  $self->delete_from_products();

  # XXX: How to handle opsyses?

  return $self->delete;
}

#########################################################################
sub update_products() {
  my $self = shift;
  my $new_product_ids = shift;
  
  if (scalar @$new_product_ids) {
    # Failing to delete products is _not_ fatal when adding a new subgroup.
    my $rv = $self->delete_from_products();
    my $dbh = __PACKAGE__->db_Main();  
    my $sql = "INSERT INTO platform_products (platform_id,product_id) VALUES (?,?)";
    foreach my $new_product_id (@$new_product_ids) {
      my $rows = $dbh->do($sql, 
			  undef,
			  $self->platform_id,
			  $new_product_id
			 );
    }
  }
}


1;
