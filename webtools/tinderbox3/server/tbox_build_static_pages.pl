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
