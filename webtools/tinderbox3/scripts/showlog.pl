#!/usr/bin/perl -wT -I.

use strict;
use CGI;
use CGI::Carp qw(fatalsToBrowser);
use Tinderbox3::Log;

my $p = new CGI;
my $machine_id = $p->param('machine_id') || "";
my $logfile = $p->param('logfile') || "";

# Detaint machine id / logfile
$machine_id =~ s/[^0-9]//g;
$logfile =~ s/[^0-9A-Za-z\.]//g;
$logfile =~ s/^\.+//g;

if (!$machine_id || !$logfile) {
  die "Must specify machine_id and logfile!";
}

my $log_fh = get_log_fh($machine_id, $logfile, "<");
if (!defined($log_fh)) {
  die "No such log found!";
}

print $p->header("text/plain");
while (<$log_fh>) {
  print;
}
close $log_fh;
