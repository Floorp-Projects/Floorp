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
 # Portions created by the Initial Developer are Copyright (C) 2005
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

Litmus::DB::Product->columns(All => qw/product_id name iconpath/);

Litmus::DB::Product->column_alias("product_id", "productid");

Litmus::DB::Product->has_many(tests => "Litmus::DB::Test");
Litmus::DB::Product->has_many(testgroups => "Litmus::DB::Testgroup");
Litmus::DB::Product->has_many(branches => "Litmus::DB::Branch");
Litmus::DB::Product->has_many(platforms => "Litmus::DB::Platform");

1;
