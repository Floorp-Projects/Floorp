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
