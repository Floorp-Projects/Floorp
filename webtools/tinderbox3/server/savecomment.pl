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
use CGI;
use Tinderbox3::Header;
use Tinderbox3::DB;
use Tinderbox3::Login;

my $p = new CGI;
my $dbh = get_dbh();

my ($login, $cookie) = check_session($p, $dbh);
header($p, $login, $cookie, "Saved Comments");

my $machine_id = $p->param('machine_id');
$machine_id =~ s/\D//g;

my $build_time = $p->param('build_time');
$build_time =~ s/\D//g;

my $tree = $p->param('tree') || "";
my $build_comment = $p->param('build_comment') || "";

if (!$build_comment) {
  die "Must enter a comment!";
}

if (!$login) {
  die "Must log in!";
}

# XXX For odd reasons, DBI doesn't want to make build_time an integer and
# Postgres don't like that, so we put it directly into the SQL statement and
# are subsequently unable to reuse the statement :(
my $sth = $dbh->prepare("INSERT INTO tbox_build_comment (machine_id, build_time, login, build_comment, comment_time) VALUES (?, " . Tinderbox3::DB::sql_abstime("?") . ", ?, ?, " . Tinderbox3::DB::sql_current_timestamp() . ")");
$sth->execute($machine_id, $build_time, $login, $build_comment);

foreach my $other_machine_id ($p->param('other_machine_id')) {
  my $other_build_time = $dbh->selectrow_arrayref("SELECT " . Tinderbox3::DB::sql_get_timestamp('build_time') . " FROM tbox_build WHERE machine_id = ? ORDER BY build_time DESC LIMIT 1", undef, $other_machine_id);
  if (defined($other_build_time)) {
    $sth->execute($other_machine_id, $other_build_time->[0], $login, $build_comment);
  }
}

Tinderbox3::DB::maybe_commit($dbh);

print "<p>Comments added.  Thank you for playing.  <a href='showbuilds.pl?tree=$tree'>View Tree</a></p>\n";

footer($p);
$dbh->disconnect;
