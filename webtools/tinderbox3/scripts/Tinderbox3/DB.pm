package Tinderbox3::DB;

use strict;
use DBI;

require Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(get_dbh);

sub get_dbh {
  my $dbh = DBI->connect("dbi:Pg:dbname=tbox", "jkeiser", "scuttlebutt", { RaiseError => 1, AutoCommit => 0 });
  return $dbh;
}

1
