#!/usr/bin/perl -wT -I.
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Tinderbox 3.
#
# The Initial Developer of the Original Code is
# John Keiser (john@johnkeiser.com).
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# ***** END LICENSE BLOCK *****

use strict;

use Getopt::Long;
use Tinderbox3::DB;
use Tinderbox3::Bonsai;
use DBI;

my %args;
$args{start} = 3;
$args{end} = 0;
GetOptions(\%args, "start:i", "end:i", "help!");
if ($args{help}) {
  print <<EOM;

Usage: tbox_bonsai_update.pl [--start=<# days ago>] [--end=<# days ago>]

Updates the bonsai cache to ensure it includes bonsai checkins from start to
end days ago.

EOM
}

my $dbh = get_dbh();

my $sth = $dbh->prepare("SELECT bonsai_id, bonsai_url, module, branch, directory, cvsroot, " . Tinderbox3::DB::sql_get_timestamp("start_cache") .  ", " . Tinderbox3::DB::sql_get_timestamp("end_cache") . " FROM tbox_bonsai");
$sth->execute();
while (my $row = $sth->fetchrow_arrayref) {
  #Tinderbox3::Bonsai::clear_cache($dbh, $row->[0]);
  Tinderbox3::Bonsai::update_cache($dbh, time - (60*60*24*$args{start}), time-(60*60*24*$args{end}), $row->[0],
                                   $row->[1], $row->[2], $row->[3], $row->[4],
                                   $row->[5], $row->[6], $row->[7]);
}
Tinderbox3::DB::maybe_commit($dbh);
$dbh->disconnect();

