#!/usr/bin/perl -w -I.
use strict;

use Getopt::Long;

my %args;
$args{uncompressed_hours} = 24;
GetOptions(\%args, "uncompressed_hours:i");

foreach my $file (glob("xml/logs/*/*.log")) {
  my @file_stat = stat($file);
  my $file_mtime = $file_stat[9];
  if ((time - $file_mtime) >= $args{uncompressed_hours}*60*60) {
    system("gzip", "-9", $file);
  }
}
