#!/usr/bin/perl -wT -I.

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
my $sth = $dbh->prepare("INSERT INTO tbox_build_comment (machine_id, build_time, login, build_comment, comment_time) VALUES (?, abstime(? + 0), ?, ?, current_timestamp())");
$sth->execute($machine_id, $build_time, $login, $build_comment);

foreach my $other_machine_id ($p->param('other_machine_id')) {
  my $other_build_time = $dbh->selectrow_arrayref("SELECT EXTRACT(EPOCH FROM build_time) FROM tbox_build WHERE machine_id = ? ORDER BY build_time DESC LIMIT 1", undef, $other_machine_id);
  if (defined($other_build_time)) {
    $sth->execute($other_machine_id, $other_build_time->[0], $login, $build_comment);
  }
}

$dbh->commit();

print "<p>Comments added.  Thank you for playing.  <a href='showbuilds.pl?tree=$tree'>View Tree</a></p>\n";

footer($p);
$dbh->disconnect;
