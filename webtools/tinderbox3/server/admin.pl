#!/usr/bin/perl -wT -I.

use strict;
use CGI;
use Tinderbox3::Header;
use Tinderbox3::DB;
use Tinderbox3::Login;

my $p = new CGI;
my $dbh = get_dbh();

my ($login, $cookie) = check_session($p, $dbh);
header($p, $login, $cookie, "Administrate Tinderbox");

print "<table class=editlist><tr><th>Trees</th></tr>\n";
my $sth = $dbh->prepare("SELECT tree_name FROM tbox_tree");
$sth->execute();
while (my $tree_info = $sth->fetchrow_arrayref()) {
  my $tree = $tree_info->[0];
  my @actions;
  push @actions, "<a href='sheriff.pl?tree=$tree'>Sheriff</a>";
  push @actions, "<a href='admintree.pl?tree=$tree'>Edit</a>";
  push @actions, "<a href='showbuilds.pl?tree=$tree'>View</a>";
  print "<tr><td><a href='showbuilds.pl?tree=$tree'>$tree</a> (" . join(' | ', @actions) . ")</td></tr>\n";
}

print "<tr><td><a href='admintree.pl'>Add Tree</a></td></tr>\n";
print "</table>\n";

footer($p);
$dbh->disconnect;
