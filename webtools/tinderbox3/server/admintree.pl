#!/usr/bin/perl -wT -I.

use CGI;
use Tinderbox3::Header;
use Tinderbox3::Actions;
use Tinderbox3::DB;
use strict;

#
# Init
#
my $p = new CGI;

my $dbh = get_dbh();
my $tree = update_tree($p, $dbh);
my $tree_str = "Edit $tree" || "Add Tree";
header($p, $tree_str);
process_actions($p, $dbh);

#
# Get the tree info to fill in the fields 
#
my $tree_info;
if (!$tree) {
  # XXX Pull these out into defaults elsewhere
  $tree_info = [ '',
                 'refcount_leaks=Lk,refcount_bloat=Bl,trace_malloc_leaks=Lk,trace_malloc_maxheap=MH,trace_malloc_allocs=A,pageload=Tp,codesize=Z,xulwinopen=Txul,startup=Ts,binary_url=Binary,warnings=Warn',
                 'refcount_leaks=Graph,refcount_bloat=Graph,trace_malloc_leaks=Graph,trace_malloc_maxheap=Graph,trace_malloc_allocs=Graph,pageload=Graph,codesize=Graph,xulwinopen=Graph,startup=Graph,binary_url=URL,warnings=Warn',
                 ];
} else {
  $tree_info = $dbh->selectrow_arrayref("SELECT password, field_short_names, field_processors FROM tbox_tree WHERE tree_name = ?", undef, $tree);
}

#
# Edit / Add tree form
#
print "<h2>$tree_str</h2>\n";

print "<p><strong><a href='admin.pl'>List Trees</a>";
if ($tree) {
  print " | <a href='sheriff.pl?tree=$tree'>Edit Sheriff / Tree Status Info</a>\n";
}
print "</strong></p>\n";

print <<EOM;
<form name=editform method=get action='admintree.pl'>
<input type=hidden name=action value='edit_tree'>
<input type=hidden name=tree value='$tree'>
<table>
<tr><th>Tree Name (this is the name used to identify the tree):</th><td><input type=text name=_tree_name value='$tree'></td></tr>
<tr><th>Password:</th><td><input type=text name=_password value='$tree_info->[0]'></td></tr>
<tr><th>Status Short Names (bloat=Bl,pageload=Tp)</th><td><input type=text name=_field_short_names size=80 value='$tree_info->[1]'></td></tr>
<tr><th>Status Handlers (bloat=Graph,binary_url=URL)</th><td><input type=text name=_field_processors size=80 value='$tree_info->[2]'></td></tr>
</table>
<input type=submit>
</form>
EOM

#
# If it's not new, have a list of patches and machines
#
if ($tree) {
  # Patch list
  print "<table class=editlist><tr><th>Patches</th></tr>\n";
  my $sth = $dbh->prepare('SELECT patch_id, patch_name FROM tbox_patch WHERE tree_name = ?');
  $sth->execute($tree);
  while (my $patch_info = $sth->fetchrow_arrayref) {
    print "<tr><td><a href='adminpatch.pl?patch_id=$patch_info->[0]'>$patch_info->[1]</a> (<a href='admintree.pl?tree=$tree&action=delete_patch&_patch_id=$patch_info->[0]'>Del</a> | <a href='admintree.pl?tree=$tree&action=delete_patch&_patch_id=$patch_info->[0]'>Obsolete</a>)</td>\n";
  }
  print "<tr><td><a href='uploadpatch.pl?tree=$tree'>Upload Patch</a></td></tr>\n";
  print "</table>\n";

  # Machine list
  print "<table class=editlist><tr><th>Machines</th></tr>\n";
  $sth = $dbh->prepare('SELECT machine_id, machine_name FROM tbox_machine WHERE tree_name = ?');
  $sth->execute($tree);
  while (my $machine_info = $sth->fetchrow_arrayref) {
    print "<tr><td><a href='adminmachine.pl?machine_id=$machine_info->[0]'>$machine_info->[1]</a></td>\n";
  }
  # XXX Add this feature in if you decide not to automatically allow machines
  # into the federation
  # print "<tr><td><a href='adminmachine.pl?tree=$tree'>Upload Machine</a></td></tr>\n";
  print "</table>\n";
}


footer($p);
$dbh->disconnect;


#
# Update / Insert the tree and perform other DB operations
#
sub update_tree {
  my ($p, $dbh) = @_;

  my $tree = $p->param('tree') || "";

  my $action = $p->param('action') || "";
  if ($action eq 'edit_tree') {
    my $newtree = $p->param('_tree_name') || "";
    my $password = $p->param('_password') || "";
    my $field_short_names = $p->param('_field_short_names') || "";
    my $field_processors = $p->param('_field_processors') || "";

    if (!$newtree) { die "Must specify a non-blank tree!"; }

    # Update or insert the tree
    if ($tree) {
      my $rows = $dbh->do("UPDATE tbox_tree SET tree_name = ?, password = ?, field_short_names = ?, field_processors = ? WHERE tree_name = ?", undef, $newtree, $password, $field_short_names, $field_processors, $tree);
      if (!$rows) {
        die "No tree named $tree!";
      }
    } else {
      my $rows = $dbh->do("INSERT INTO tbox_tree (tree_name, password, field_short_names, field_processors) VALUES (?, ?, ?, ?)", undef, $newtree, $password, $field_short_names, $field_processors);
      if (!$rows) {
        die "Passing strange.  Insert failed.";
      }
      $tree = $newtree;
    }
    $dbh->commit;
  } elsif ($action eq 'delete_patch') {
    my $patch_id = $p->param('_patch_id') || "";
    if (!$patch_id) { die "Need patch id!" }
    my $rows = $dbh->do("DELETE FROM tbox_patch WHERE tree_name = ? AND patch_id = ?", undef, $tree, $patch_id);
    if (!$rows) {
      die "Delete failed.  No such tree / patch.";
    }
    $dbh->commit;
  } elsif ($action eq 'obsolete_patch') {
    my $patch_id = $p->param('_patch_id') || "";
    if (!$patch_id) { die "Need patch id!" }
    my $rows = $dbh->do("UPDATE tbox_patch SET obsolete = 'Y' WHERE tree_name = ? AND patch_id = ?", undef, $tree, $patch_id);
    if (!$rows) {
      die "Update failed.  No such tree / patch.";
    }
    $dbh->commit;
  }

  return $tree;
}

