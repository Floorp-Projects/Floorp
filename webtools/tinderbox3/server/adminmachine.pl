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

use CGI;
use Tinderbox3::Header;
use Tinderbox3::DB;
use Tinderbox3::Login;
use strict;

#
# Init
#
my $p = new CGI;
my $dbh = get_dbh();
my ($login, $cookie) = check_session($p, $dbh);

my $tree = $p->param('tree') || "";

# For edit_machine
my $machine_id = Tinderbox3::DB::update_machine_action($p, $dbh, $login);

# Get patch from DB
my $machine_info = $dbh->selectrow_arrayref("SELECT tree_name, machine_name, os, os_version, compiler, clobber, commands, visible FROM tbox_machine WHERE machine_id = ?", undef, $machine_id);
if (!defined($machine_info)) {
  die "Could not get machine!";
}
my ($machine_name, $os, $os_version, $compiler, $clobber, $commands, $visible);
($tree, $machine_name, $os, $os_version, $compiler, $clobber, $commands, $visible) = @{$machine_info};

my %machine_config;
my $sth = $dbh->prepare("SELECT name, value FROM tbox_machine_config WHERE machine_id = ? ORDER BY name");
$sth->execute($machine_id);
while (my $row = $sth->fetchrow_arrayref) {
  $machine_config{$row->[0]} = $row->[1];
}

header($p, $login, $cookie, "Edit Machine $machine_name", $tree, $machine_id, $machine_name);

#
# Edit patch form
#

print <<EOM;
<form name=editform method=get action='adminmachine.pl'>
<input type=hidden name=action value='edit_machine'>
@{[$p->hidden(-name=>'tree', -default=>$tree)]}
@{[$p->hidden(-name=>'machine_id', -default=>$machine_id)]}
<table>
<tr><th>Machine Name:</th><td>@{[$p->escapeHTML($machine_name)]}</td></tr>
<tr><th>OS:</th><td>@{[$p->escapeHTML("$os $os_version")]}</td></tr>
<tr><th>Compiler:</th><td>@{[$p->escapeHTML($compiler)]}</td></tr>
<tr><th>Clobber:</th><td>@{[$clobber ? 'Clobber' : 'Depend']}</td></tr>
<tr><th>Commands</th><td>@{[$p->textfield(-name=>'commands', -default=>$commands)]}</td></tr>
<tr><th>Visible:</th><td><input type=checkbox name=visible@{[$visible ? " checked" : ""]}><td></tr>
</table>

<strong>.mozconfig (set to blank to use default):</strong><br>
<input type=hidden name=machine_config0 value=mozconfig>
@{[$p->textarea(-name=>'machine_config0_val', -default=>$machine_config{mozconfig}, -rows=>5,
                -columns=>100)]}<br>
EOM

print "<p><strong>Machine Config:</strong><br>";
print "(Empty a line to use default for tree)<br>";
print "<table><tr><th>Var</th><th>Value</th></tr>\n";
my $config_num = 1;
foreach my $var (sort keys %machine_config) {
  my $value = $machine_config{$var};
  if ($var ne "mozconfig") {
    print "<tr><td>", $p->textfield(-name=>"machine_config$config_num", -default=>$var, -override=>1), "</td>";
    print "<td>", $p->textfield(-name=>"machine_config${config_num}_val", -default=>$value, -override=>1), "</td></tr>\n";
    $config_num++;
  }
}
foreach my $i ($config_num..($config_num+2)) {
    print "<tr><td>", $p->textfield(-name=>"machine_config$i", -override=>1), "</td>";
    print "<td>", $p->textfield(-name=>"machine_config${i}_val", -override=>1), "</td></tr>\n";
}
print "</table></p>\n";

if (!$login) {
  print login_fields();
}

print <<EOM;
<input type=submit>
</form>
<form action='admintree.pl'>@{[$p->hidden(-name => 'tree', -default => $tree, -override => 1)]}@{[$p->hidden(-name => 'action', -default => 'delete_machine', -override => 1)]}@{[$p->hidden(-name => 'machine_id', -default => $machine_id, -override => 1)]}<input type=submit value='DELETE this machine and ALL logs associated with it' onclick='return confirm("Dude.  Seriously, this will wipe out all the logs and fields and everything associated with this machine.  Think hard here.\\n\\nDo you really want to do this?")'>
</form>
EOM


footer($p);
$dbh->disconnect;
