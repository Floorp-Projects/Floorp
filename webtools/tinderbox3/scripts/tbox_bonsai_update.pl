#!/usr/bin/perl -wT -I.
use strict;

use Tinderbox3::DB;
use Tinderbox3::Bonsai;
use DBI;

my $dbh = get_dbh();

my $sth = $dbh->prepare("SELECT bonsai_id, bonsai_url, module, branch, directory, cvsroot, EXTRACT(EPOCH FROM start_cache), EXTRACT(EPOCH FROM end_cache) FROM tbox_bonsai");
$sth->execute();
while (my $row = $sth->fetchrow_arrayref) {
  #Tinderbox3::Bonsai::clear_cache($dbh, $row->[0]);
  Tinderbox3::Bonsai::update_cache($dbh, time - (60*60*24*2), time, $row->[0],
                                   $row->[1], $row->[2], $row->[3], $row->[4],
                                   $row->[5], $row->[6], $row->[7]);
}
$dbh->commit();
$dbh->disconnect();

