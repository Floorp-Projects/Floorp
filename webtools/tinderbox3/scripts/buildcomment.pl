#!/usr/bin/perl -wT -I.

use strict;
use CGI;
use Tinderbox3::Header;
use Tinderbox3::DB;
use Tinderbox3::Login;

my $p = new CGI;
my $dbh = get_dbh();

my ($login, $cookie) = check_session($p, $dbh);
header($p, $login, $cookie, "Add Comments");

my $machine_id = $p->param('machine_id');
$machine_id =~ s/\D//g;

my $build_time = $p->param('build_time');
$build_time =~ s/\D//g;

my $tree = $p->param('tree');

print qq^<form action='savecomment.pl'>
@{[$p->hidden(-name=>'tree', -default=>$tree)]}
<input type=hidden name=machine_id value='$machine_id'>
<input type=hidden name=build_time value='$build_time'>
<p>
^;
if (!$login) {
  print login_fields(), "<br>\n";;
} else {
  print "<strong>Email:</strong> " . $login . "<br>\n";
}
print "<textarea name=build_comment rows=10 cols=30></textarea><br>\n";
print "<input type=submit>\n</p>\n";

print "<p>Other builds to add comment to (will be added to most recent cycle):</p>\n";
my $sth = $dbh->prepare("SELECT machine_id, machine_name FROM tbox_machine WHERE tree_name = ? AND visible");
$sth->execute($tree);
while (my $row = $sth->fetchrow_arrayref()) {
  next if $row->[0] == $machine_id;
  print "<input type=checkbox name=other_machine_id value='$row->[0]'> $row->[1]\n";
}

print "</form>\n";

footer($p);
$dbh->disconnect;
