#!/usr/bin/perl -wT -I.

use strict;
use CGI;
use Tinderbox3::DB;
use Tinderbox3::ShowBuilds;

open INDEX_FILE, ">index.html";

print INDEX_FILE <<EOM;
<html>
<head>
<title>Tinderbox - Index</title>
</head>
<body>
<h2>Tinderbox - Index</h2>
<p><a href='admin.pl'>Administrate This Tinderbox</a></p>
<p>This Tinderbox has the following trees:</p>
EOM

#
# Create the actual tree static pages
#
my $p = new CGI;
my $dbh = get_dbh();

my $trees = $dbh->selectcol_arrayref("SELECT tree_name FROM tbox_tree");
foreach my $tree (@{$trees}) {
  my $end_time = time;
  my $start_time = time - 24*60*60;

  open OUTFILE, ">$tree.html";
  Tinderbox3::ShowBuilds::print_showbuilds($p, $dbh, *OUTFILE, $tree,
                                           $start_time, $end_time);
  close OUTFILE;

  print INDEX_FILE "<a href='$tree.html'>$tree</a> (<a href='showbuilds.pl?tree=$tree'><a href='showbuilds.pl?tree=$tree'>Dynamic</a>)<br>\n";
}

print INDEX_FILE "</body>
</html>";
close INDEX_FILE;

$dbh->disconnect;
