#!/usr/bin/perl -wT -I.

use strict;
use CGI;
use Tinderbox3::Header;
use Tinderbox3::DB;

my $p = new CGI;
my $dbh = get_dbh();
header($p, "Global Admin");

print "<h2>Administrate Tinderbox</h2>\n";

print "<table class=editlist><tr><th>Trees</th></tr>\n";
foreach my $tree (@{$dbh->selectcol_arrayref("SELECT tree_name FROM tbox_tree")}) {
  print "<tr><td><a href='admintree.pl?tree=$tree'>$tree</a></td></tr>\n";
}

print "<tr><td><a href='admintree.pl?tree='>Add Tree</a></td></tr>\n";
print "</table>\n";

footer($p);
$dbh->disconnect;
