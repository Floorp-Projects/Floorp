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
use Tinderbox3::InitialValues;
use Tinderbox3::Login;
use strict;

#
# Init
#
my $p = new CGI;
my $dbh = get_dbh();
my ($login, $cookie) = check_session($p, $dbh);

# For delete_machine
Tinderbox3::DB::update_machine_action($p, $dbh, $login);
# For delete_patch, stop_using_patch
Tinderbox3::DB::update_patch_action($p, $dbh, $login);
# For delete_bonsai
Tinderbox3::DB::update_bonsai_action($p, $dbh, $login);
# For edit_tree
my $tree = Tinderbox3::DB::update_tree_action($p, $dbh, $login);

#
# Get the tree info to fill in the fields 
#
my $tree_info;
my %initial_machine_config;
if (!$tree) {
  $tree_info = [ $Tinderbox3::InitialValues::field_short_names,
                 $Tinderbox3::InitialValues::field_processors,
                 $Tinderbox3::InitialValues::statuses,
                 $Tinderbox3::InitialValues::min_row_size,
                 $Tinderbox3::InitialValues::max_row_size,
                 $Tinderbox3::InitialValues::default_tinderbox_view,
                 Tinderbox3::DB::sql_get_bool($Tinderbox3::InitialValues::new_machines_visible),
                 '',
                 ];
  %initial_machine_config = %Tinderbox3::InitialValues::initial_machine_config;
} else {
  $tree_info = $dbh->selectrow_arrayref("SELECT field_short_names, field_processors, statuses, min_row_size, max_row_size, default_tinderbox_view, new_machines_visible, editors FROM tbox_tree WHERE tree_name = ?", undef, $tree);
  if (!defined($tree_info)) {
    die "Could not get tree!";
  }

  my $sth = $dbh->prepare("SELECT name, value FROM tbox_initial_machine_config WHERE tree_name = ?");
  $sth->execute($tree);
  while (my $row = $sth->fetchrow_arrayref) {
    $initial_machine_config{$row->[0]} = $row->[1];
  }
}

#
# Edit / Add tree form
#
header($p, $login, $cookie, ($tree ? "Edit $tree" : "Add Tree"), $tree);

print <<EOM;
<form name=editform method=get action='admintree.pl'>
<input type=hidden name=action value='edit_tree'>
@{[$p->hidden(-name=>'tree', -default=>$tree, -override=>1)]}
<table>
<tr><th>Tree Name (this is the name used to identify the tree):</th><td>@{[$p->textfield(-name=>'tree_name', -default=>$tree)]}</td></tr>
<tr><th>Status Short Names (bloat=Bl,pageload=Tp...)</th><td>@{[$p->textfield(-name=>'field_short_names', -default=>$tree_info->[0], -size=>80)]}</td></tr>
<tr><th>Status Handlers (bloat=Graph,binary_url=URL...)</th><td>@{[$p->textfield(-name=>'field_processors', -default=>$tree_info->[1], -size=>80)]}</td></tr>
<tr><th>Tree Statuses (open,closed...)</th><td>@{[$p->textfield(-name=>'statuses', -default=>$tree_info->[2], -size=>80)]}</td></tr>
<tr><th>Min Row Size (minutes)</th><td>@{[$p->textfield(-name=>'min_row_size', -default=>$tree_info->[3])]}</td></tr>
<tr><th>Max Row Size (minutes)</th><td>@{[$p->textfield(-name=>'max_row_size', -default=>$tree_info->[4])]}</td></tr>
<tr><th>Tinderbox Page Size (minutes)</th><td>@{[$p->textfield(-name=>'default_tinderbox_view', -default=>$tree_info->[5])]}</td></tr>
<tr><th>New Machines Visible By Default?</th><td><input type=checkbox name=new_machines_visible@{[$tree_info->[6] ? ' checked' : '']}></td></tr>
<tr><th>Editor Privileges (list of emails)</th><td>@{[$p->textfield(-name=>'editors', -default=>$tree_info->[7])]}</td></tr>
</table>
<p><strong>Initial .mozconfig:</strong><br>
<input type=hidden name=initial_machine_config0 value=mozconfig>
@{[$p->textarea(-name=>'initial_machine_config0_val', -default=>$initial_machine_config{mozconfig}, -rows=>5, -columns => 100)]}</p>
EOM

print "<p><strong>Initial Machine Config:</strong><br>";
print "(Empty a line to delete it)<br>";
print "<table><tr><th>Var</th><th>Value</th></tr>\n";
my $config_num = 1;
foreach my $var (sort keys %initial_machine_config) {
  my $value = $initial_machine_config{$var};
  if ($var ne "mozconfig") {
    print "<tr><td>", $p->textfield(-name=>"initial_machine_config$config_num", -default=>$var, -override=>1), "</td>";
    print "<td>", $p->textfield(-name=>"initial_machine_config${config_num}_val", -default=>$value, -override=>1), "</td></tr>\n";
    $config_num++;
  }
}
foreach my $i ($config_num..($config_num+2)) {
    print "<tr><td>", $p->textfield(-name=>"initial_machine_config$i", -override=>1), "</td>";
    print "<td>", $p->textfield(-name=>"initial_machine_config${i}_val", -override=>1), "</td></tr>\n";
}
print "</table></p>\n";

if (!$login) {
  print login_fields();
}

print <<EOM;
<input type=submit>
</form>
EOM

#
# If it's not new, have a list of patches and machines
#
if ($tree) {
  # Patch list
  print "<table class=editlist><tr><th>Patches</th></tr>\n";
  my $sth = $dbh->prepare('SELECT patch_id, patch_name, in_use FROM tbox_patch WHERE tree_name = ? ORDER BY patch_name');
  $sth->execute($tree);
  while (my $patch_info = $sth->fetchrow_arrayref) {
    my ($patch_class, $action, $action_name);
    if ($patch_info->[2]) {
      $patch_class = "";
      $action = "stop_using_patch";
      $action_name = "Obsolete";
    } else {
      $patch_class = " class=obsolete";
      $action = "start_using_patch";
      $action_name = "Resurrect";
    }
    print "<tr><td><a href='adminpatch.pl?patch_id=$patch_info->[0]'$patch_class>$patch_info->[1]</a> (<a href='admintree.pl?tree=$tree&action=delete_patch&patch_id=$patch_info->[0]'>Del</a> | <a href='admintree.pl?tree=$tree&action=$action&patch_id=$patch_info->[0]'>$action_name</a>)</td>\n";
  }
  print "<tr><td><a href='uploadpatch.pl?tree=$tree'>Upload Patch</a></td></tr>\n";
  print "</table>\n";

  # Machine list
  print "<table class=editlist><tr><th>Machines</th></tr>\n";
  $sth = $dbh->prepare('SELECT machine_id, machine_name FROM tbox_machine WHERE tree_name = ? ORDER BY machine_name');
  $sth->execute($tree);
  while (my $machine_info = $sth->fetchrow_arrayref) {
    print "<tr><td><a href='adminmachine.pl?tree=$tree&machine_id=$machine_info->[0]'>$machine_info->[1]</a></td>\n";
  }
  # XXX Add this feature in if you decide not to automatically allow machines
  # into the federation
  # print "<tr><td><a href='adminmachine.pl?tree=$tree'>New Machine</a></td></tr>\n";
  print "</table>\n";

  # Machine list
  print "<table class=editlist><tr><th>Bonsai Monitors</th></tr>\n";
  $sth = $dbh->prepare('SELECT bonsai_id, display_name FROM tbox_bonsai WHERE tree_name = ? ORDER BY display_name');
  $sth->execute($tree);
  while (my $bonsai_info = $sth->fetchrow_arrayref) {
    print "<tr><td><a href='adminbonsai.pl?tree=$tree&bonsai_id=$bonsai_info->[0]'>$bonsai_info->[1]</a> (<a href='admintree.pl?tree=$tree&action=delete_bonsai&bonsai_id=$bonsai_info->[0]'>Del</a>)</td>\n";
  }
  print "<tr><td><a href='adminbonsai.pl?tree=$tree'>New Bonsai</a></td></tr>\n";
  print "</table>\n";
}


footer($p);
$dbh->disconnect;
