#!/usr/bin/perl -wT -I.

use strict;
use CGI;
use Tinderbox3::DB;
use Tinderbox3::ShowBuilds;

my $p = new CGI;
my $dbh = get_dbh();

my $tree = $p->param('tree') || "";
my ($start_time, $end_time);
if ($p->param('start_time')) {
  $start_time = $p->param('start_time');
  if ($start_time > time) {
    $start_time = time;
  }
  $end_time = $start_time + ($p->param('interval') || (24*60*60));
  if ($end_time > time) {
    $end_time = time;
  }
} else {
  $end_time = time;
  $start_time = $end_time - 24*60*60;
}

my $min_row_size = $p->param('min_row_size');
my $max_row_size = $p->param('max_row_size');

print $p->header;
Tinderbox3::ShowBuilds::print_showbuilds($p, $dbh, *STDOUT, $tree, $start_time,
                                         $end_time, $min_row_size, $max_row_size);

$dbh->disconnect;
