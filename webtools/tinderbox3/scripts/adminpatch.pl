#!/usr/bin/perl -wT -I.

use CGI;
use Tinderbox3::Header;
use Tinderbox3::DB;
use strict;

#
# Init
#
my $p = new CGI;

my $dbh = get_dbh();
my ($patch_id, $message) = update_patch($p, $dbh);
# Get patch from DB
my $patch_info = $dbh->selectrow_arrayref("SELECT tree_name, patch_name, patch_ref, patch, obsolete FROM tbox_patch WHERE patch_id = ?");
if (!$patch_info) {
  die "Could not get patch!";
}
my ($tree, $patch_name, $patch_ref, $patch, $obsolete) = @{$patch_info};
my $bug_id;
if ($patch_ref =~ /Bug\s+(.*)/) {
  $bug_id = $1;
}

header($p, "Edit Patch $patch_name");

#
# Edit / Add tree form
#
print "<h2>Edit Patch $patch_name</h2>\n";

print "<p><strong><a href='admin.pl'>List Trees</a>";
if ($tree) {
  print " | <a href='sheriff.pl?tree=$tree'>Edit Sheriff / Tree Status Info</a> | <a href='admintree.pl?tree=$tree'>Edit Tree</a>\n";
}
print "</strong></p>\n";

print <<EOM;
<form name=editform method=get action='adminpatch.pl'>
<input type=hidden name=action value='edit_patch'>
<input type=hidden name=patch_id value='$patch_id'>
<table>
<tr><th>Patch Name (just for display):</th><td><input type=text name=_patch_name value='$patch_name'></td></tr>
<tr><th>Bug #:</th><td><input type=text name=_patch_ref value='$bug_id'></td></tr>
</table>
<input type=submit>
</form>
EOM


footer($p);
$dbh->disconnect;


#
# Update / insert the patch
#
sub update_patch {
  my ($p, $dbh) = @_;

  my $tree = $p->param('tree') || "";

  my $action = $p->param('action') || "";
  if ($action eq 'upload_patch') {
    my $tree = $p->param('tree') || "";
    my $patch_name = $p->param('_patch_name') || "";
    my $bug_id = $p->param('_bug_id') || "";

    if (!$patch_name) { die "Must specify a non-blank patch name!"; }

    my $patch_fh = $p->upload('_patch');
    if (!$patch_fh) { die "No patch file uploaded!"; }
    my $patch = "";
    while (<$patch_fh>) {
      $patch .= $_;
    }

    my $rows = $dbh->do("INSERT INTO tbox_patch (tree_name, patch_name, patch_ref, patch_ref_url, patch) VALUES (?, ?, ?, ?, ?)", undef, $tree, $patch_name, "Bug $bug_id", "http://bugzilla.mozilla.org/show_bug.cgi?id=$bug_id", $patch);

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

