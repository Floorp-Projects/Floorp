#!/usr/bin/perl -wT -I..
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
use CGI;
use Tinderbox3::DB;
use Tinderbox3::XML;
use Tinderbox3::Log;


our $p = new CGI();
our $dbh = get_dbh();

$SIG{__DIE__} = sub { die_xml_error($p, $dbh, $_[0]); };

my $tree = $p->param('tree') || "";
if (!$tree) {
  die_xml_error($p, $dbh, "Must specify tree!");
}
my $machine_name = $p->param('machine_name') || "";
if (!$machine_name) {
  die_xml_error($p, $dbh, "Must specify a machine name!");
}
my $os = $p->param('os') || "";
my $os_version = $p->param('os_version') || "";
my $compiler = $p->param('compiler') || "";
my $status = $p->param('status') || 0;
my $clobber = $p->param('clobber') || 0;

#
# Get data for response
#
my $tree_info = $dbh->selectrow_arrayref("SELECT new_machines_visible FROM tbox_tree WHERE tree_name = ?", undef, $tree);
if (!defined($tree_info)) {
  die_xml_error($p, $dbh, "Could not get tree!");
}
my ($new_machines_visible) = @{$tree_info};

my $patch_ids = $dbh->selectcol_arrayref("SELECT patch_id FROM tbox_patch WHERE tree_name = ? AND in_use", undef, $tree);
if (!$patch_ids) {
  die_xml_error($p, $dbh, "Could not get patches!");
}

#
# Insert the machine into the machines table if it is not there
#
my $machine_info = $dbh->selectrow_arrayref("SELECT machine_id, commands FROM tbox_machine WHERE tree_name = ? AND machine_name = ? AND os = ? AND os_version = ? AND compiler = ?", undef, $tree, $machine_name, $os, $os_version, $compiler);
if (!defined($machine_info)) {
  $dbh->do("INSERT INTO tbox_machine (tree_name, machine_name, visible, os, os_version, compiler, clobber) VALUES (?, ?, ?, ?, ?, ?, ?)", undef, $tree, $machine_name, $new_machines_visible, $os, $os_version, $compiler, Tinderbox3::DB::sql_get_bool($clobber));
  $machine_info = [ Tinderbox3::DB::sql_get_last_id($dbh, 'tbox_machine_machine_id_seq'), "" ]
} else {
  $dbh->do("UPDATE tbox_machine SET clobber = ? WHERE machine_id = ?", undef, Tinderbox3::DB::sql_get_bool($clobber), $machine_info->[0]);
}
my ($machine_id, $commands) = @{$machine_info};
$commands ||= "";

$machine_id =~ /(\d+)/;
$machine_id = $1;

#
# Get the machine config
#
my %machine_config;
my $sth = $dbh->prepare("SELECT name, value FROM tbox_initial_machine_config WHERE tree_name = ?");
$sth->execute($tree);
while (my $row = $sth->fetchrow_arrayref()) {
  $machine_config{$row->[0]} = $row->[1];
}
$sth = $dbh->prepare("SELECT name, value FROM tbox_machine_config WHERE machine_id = ?");
$sth->execute($machine_id);
while (my $row = $sth->fetchrow_arrayref()) {
  $machine_config{$row->[0]} = $row->[1];
}

{
  #
  # Close the last old build info if there is one and it was incomplete
  #
  my $last_build = $dbh->selectrow_arrayref("SELECT status, build_time, log FROM tbox_build WHERE machine_id = ? ORDER BY build_time DESC LIMIT 1", undef, $machine_id);
  if (defined($last_build) && $last_build->[0] >= 0 &&
      $last_build->[0] < 100) {
    my $rows = $dbh->do("UPDATE tbox_build SET status = ? WHERE machine_id = ? AND build_time = ?", undef, $last_build->[0] + 300, $machine_id, $last_build->[1]);
    # We have to compress the log too, be a good citizen
    compress_log($machine_id, $last_build->[2]);
  }

  # Create logfile
  my $log = create_logfile_name($machine_id);
  my $fh = get_log_fh($machine_id, $log, ">");
  close $fh;

  #
  # Insert a new build info signifying that the build has started
  #
  my $timestamp = Tinderbox3::DB::sql_current_timestamp();
  $dbh->do("INSERT INTO tbox_build (machine_id, build_time, status_time, status, log) VALUES (?, $timestamp, $timestamp, ?, ?)", undef, $machine_id, $status, $log);
}

#
# If there are commands, we have delivered them.  Set to blank.
#
if ($commands) {
  $dbh->do("UPDATE tbox_machine SET commands = '' WHERE machine_id = $machine_id");
}

Tinderbox3::DB::maybe_commit($dbh);

#
# Print response
#
print $p->header("text/xml");
print "<response>\n";
print "<tree name='$tree'>\n";
foreach my $patch_id (@{$patch_ids}) {
  print "<patch id='$patch_id'/>\n";
}
print "</tree>\n";
print "<machine id='$machine_info->[0]'>\n";
print "<commands>", $p->escapeHTML($commands), "</commands>\n";
while (my ($var, $val) = each %machine_config) {
  print "<$var>", $p->escapeHTML($val), "</$var>\n";
}
print "</machine>\n";
print "</response>\n";

$dbh->disconnect;
