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
