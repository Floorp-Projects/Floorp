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

package Tinderbox3::DB;

use strict;
use DBI;
use Tinderbox3::InitialValues;
use Tinderbox3::Bonsai;
use Tinderbox3::Login;
use Tinderbox3::Log;

require Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(get_dbh sql_current_timestamp sql_abstime sql_get_last_id sql_get_timestamp sql_get_bool);

# dbtype = mysql or Pg
our $dbtype = "mysql";
our $dbname = "tbox3";
our $username = "";
our $password = "";
sub get_dbh {
  my $dbh = DBI->connect("dbi:$dbtype:dbname=$dbname", $username, $password, { RaiseError => 1 });
  return $dbh;
}

sub maybe_commit {
  my ($dbh) = @_;
  # $dbh->commit();
}

sub check_edit_tree {
  my ($login, $dbh, $tree, $action) = @_;
  my $row = $dbh->selectrow_arrayref("SELECT editors FROM tbox_tree WHERE tree_name = ?", undef, $tree);
  if (!can_edit_tree($login, $row->[0])) {
    die "$login: Insufficient privileges to $action (need edit tree)!";
  }
}

sub check_sheriff_tree {
  my ($login, $dbh, $tree, $action) = @_;
  my $row = $dbh->selectrow_arrayref("SELECT editors, sheriffs FROM tbox_tree WHERE tree_name = ?", undef, $tree);
  if (!can_sheriff_tree($login, $row->[0], $row->[1])) {
    die "$login: Insufficient privileges to $action (need sheriff tree)!";
  }
}

sub check_edit_patch {
  my ($login, $dbh, $patch_id, $action) = @_;
  my $row = $dbh->selectrow_arrayref("SELECT t.editors FROM tbox_patch p, tbox_tree t WHERE p.patch_id = ? AND t.tree_name = p.tree_name", undef, $patch_id);
  if (!can_edit_tree($login, $row->[0])) {
    die "$login: Insufficient privileges to $action (need edit tree)!";
  }
}

sub check_edit_machine {
  my ($login, $dbh, $machine_id, $action) = @_;
  my $row = $dbh->selectrow_arrayref("SELECT t.editors FROM tbox_machine m, tbox_tree t WHERE m.machine_id = ? AND t.tree_name = m.tree_name", undef, $machine_id);
  if (!can_edit_tree($login, $row->[0])) {
    die "$login: Insufficient privileges to $action (need edit tree)!";
  }
}

sub check_edit_bonsai {
  my ($login, $dbh, $bonsai_id) = @_;
  my $row = $dbh->selectrow_arrayref("SELECT t.editors FROM tbox_bonsai b, tbox_tree t WHERE b.bonsai_id = ? AND t.tree_name = b.tree_name", undef, $bonsai_id);
  if (!can_edit_tree($login, $row->[0])) {
    die "Insufficient privileges to edit bonsai (need edit tree)!";
  }
}

#
# Perform the upload_patch or edit_patch action
#
sub update_patch_action {
  my ($p, $dbh, $login) = @_;

  my $patch_id = $p->param('patch_id') || "";
  my $action = $p->param('action') || "";
  if ($action eq 'upload_patch' || $action eq 'edit_patch') {
    my $patch_name = $p->param('patch_name') || "";
    my $bug_id = $p->param('bug_id') || "";
    my $in_use = sql_get_bool($p->param('in_use'));
    my $patch_ref = "Bug $bug_id";
    my $patch_ref_url = "http://bugzilla.mozilla.org/show_bug.cgi?id=$bug_id";

    if (!$patch_name) { die "Must specify a non-blank patch name!"; }

    if ($action eq 'upload_patch') {
      # Check security
      my $tree = $p->param('tree') || "";
      check_edit_tree($login, $dbh, $tree, "upload patch");

      # Get patch
      my $patch_fh = $p->upload('patch');
      if (!$patch_fh) { die "No patch file uploaded!"; }
      my $patch = "";
      while (<$patch_fh>) {
        $patch .= $_;
      }

      # Perform patch insert
      $dbh->do("INSERT INTO tbox_patch (tree_name, patch_name, patch_ref, patch_ref_url, patch, in_use) VALUES (?, ?, ?, ?, ?, ?)", undef, $tree, $patch_name, $patch_ref, $patch_ref_url, $patch, $in_use);
      maybe_commit($dbh);

    } else {
      # Check security
      check_edit_patch($login, $dbh, $patch_id, "edit patch");

      # Perform patch update
      my $rows = $dbh->do("UPDATE tbox_patch SET patch_name = ?, patch_ref = ?, patch_ref_url = ?, in_use = ? WHERE patch_id = ?", undef, $patch_name, $patch_ref, $patch_ref_url, $in_use, $patch_id);
      if (!$rows) {
        die "Could not find patch!";
      }
      maybe_commit($dbh);
    }

  } elsif ($action eq 'delete_patch') {
    if (!$patch_id) { die "Need patch id!"; }

    # Check security
    check_edit_patch($login, $dbh, $patch_id, "delete patch");

    # Perform patch delete
    my $rows = $dbh->do("DELETE FROM tbox_patch WHERE patch_id = ?", undef, $patch_id);
    if (!$rows) {
      die "Delete failed.  No such tree / patch.";
    }
    maybe_commit($dbh);

  } elsif ($action eq 'stop_using_patch' || $action eq 'start_using_patch') {
    # Check security
    check_edit_patch($login, $dbh, $patch_id, "start/stop using patch");

    if (!$patch_id) { die "Need patch id!" }
    my $rows = $dbh->do("UPDATE tbox_patch SET in_use = ? WHERE patch_id = ?", undef, sql_get_bool($action eq 'start_using_patch'), $patch_id);
    if (!$rows) {
      die "Update failed.  No such tree / patch.";
    }
    maybe_commit($dbh);
  }

  return $patch_id;
}

#
# Update / Insert the tree and perform other DB operations
#
sub update_tree_action {
  my ($p, $dbh, $login) = @_;

  my $tree = $p->param('tree') || "";

  my $action = $p->param('action') || "";
  if ($action eq 'edit_tree') {
    my $newtree = $p->param('tree_name') || "";
    my $field_short_names = $p->param('field_short_names') || "";
    my $field_processors = $p->param('field_processors') || "";
    my $statuses = $p->param('statuses') || "";
    my $min_row_size = $p->param('min_row_size') || "0";
    my $max_row_size = $p->param('max_row_size') || "0";
    my $default_tinderbox_view = $p->param('default_tinderbox_view') || "0";
    my $new_machines_visible = sql_get_bool($p->param('new_machines_visible'));
    my $editors = $p->param('editors') || "";

    if (!$newtree) { die "Must specify a non-blank tree!"; }

    # Update or insert the tree
    if ($tree) {
      # Check security
      check_edit_tree($login, $dbh, $tree, "edit tree");

      # Perform tree update
      my $rows = $dbh->do("UPDATE tbox_tree SET tree_name = ?, field_short_names = ?, field_processors = ?, statuses = ?, min_row_size = ?, max_row_size = ?, default_tinderbox_view = ?, new_machines_visible = ?, editors = ? WHERE tree_name = ?", undef, 
      $newtree, $field_short_names, $field_processors, $statuses,
      $min_row_size, $max_row_size, $default_tinderbox_view,
      $new_machines_visible, $editors,
      $tree);
      if (!$rows) {
        die "No tree named $tree!";
      }
    } else {
      # Check security
      if (!can_admin($login)) {
        die "Insufficient privileges to add tree!  (Need superuser)";
      }

      # Perform tree insert
      my $rows = $dbh->do("INSERT INTO tbox_tree (tree_name, field_short_names, field_processors, statuses, min_row_size, max_row_size, default_tinderbox_view, new_machines_visible, editors, header, footer, special_message, sheriff, build_engineer, status, sheriffs) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", undef,
      $newtree, $field_short_names, $field_processors, $statuses,
      $min_row_size, $max_row_size, $default_tinderbox_view,
      $new_machines_visible, $editors,
      $Tinderbox3::InitialValues::header,
      $Tinderbox3::InitialValues::footer,
      $Tinderbox3::InitialValues::special_message,
      $Tinderbox3::InitialValues::sheriff,
      $Tinderbox3::InitialValues::build_engineer,
      $Tinderbox3::InitialValues::status,
      '');
      if (!$rows) {
        die "Passing strange.  Insert failed.";
      }
      $tree = $newtree;
    }

    # Update initial config values
    $dbh->do("DELETE FROM tbox_initial_machine_config WHERE tree_name = ?", undef, $newtree);
    my $i = 0;
    my $sth = $dbh->prepare("INSERT INTO tbox_initial_machine_config (tree_name, name, value) VALUES (?, ?, ?)");
    while (defined($p->param("initial_machine_config$i"))) {
      my $var = $p->param("initial_machine_config$i");
      if ($var) {
        my $val = $p->param("initial_machine_config${i}_val");
        $val =~ s/\r//g;
        $sth->execute($newtree, $var, $val);
      }
      $i++;
    }

    maybe_commit($dbh);

    # Return the new tree name
    $tree = $newtree;
  } elsif ($action eq 'edit_sheriff') {
    # Check security
    check_sheriff_tree($login, $dbh, $tree, "sheriff tree");

    my $header = $p->param('header') || "";
    my $footer = $p->param('footer') || "";
    my $special_message = $p->param('special_message') || "";
    my $sheriff = $p->param('sheriff') || "";
    my $build_engineer = $p->param('build_engineer') || "";
    my $status = $p->param('status') || "";
    my $sheriffs = $p->param('sheriffs') || "";
    my $rows = $dbh->do("UPDATE tbox_tree SET header = ?, footer = ?, special_message = ?, sheriff = ?, build_engineer = ?, status = ?, sheriffs = ? WHERE tree_name = ?", undef, 
      $header, $footer, $special_message, $sheriff, $build_engineer, $status,
      $sheriffs,
      $tree);
    if (!$rows) {
      die "No tree named $tree!";
    }
    maybe_commit($dbh);
  }

  return $tree;
}

#
# Update machine information
#
sub update_machine_action {
  my ($p, $dbh, $login) = @_;

  my $machine_id = $p->param('machine_id') || "";
  $machine_id = $1 if $machine_id =~ /(\d+)/;

  my $action = $p->param('action') || "";
  if ($action eq 'edit_machine') {
    die "Must pass machine_id!" if !$machine_id;

    # Check security
    check_edit_machine($login, $dbh, $machine_id, "edit machine");

    my $visible = sql_get_bool($p->param('visible'));
    my $commands = $p->param('commands');

    my $rows = $dbh->do('UPDATE tbox_machine SET visible = ?, commands = ? WHERE machine_id = ?', undef, $visible, $commands, $machine_id);
    if (!$rows) {
      die "Could not update machine!";
    }

    # Update config values
    $dbh->do("DELETE FROM tbox_machine_config WHERE machine_id = ?", undef, $machine_id);
    my $i = 0;
    my $sth = $dbh->prepare("INSERT INTO tbox_machine_config (machine_id, name, value) VALUES (?, ?, ?)");
    while (defined($p->param("machine_config$i"))) {
      my $var = $p->param("machine_config$i");
      if ($var) {
        my $val = $p->param("machine_config${i}_val");
        # Don't put mozconfig in the table if the value is empty
        if (!($var eq "mozconfig" && !$val)) {
          $val =~ s/\r//g;
          $sth->execute($machine_id, $var, $val);
        }
      }
      $i++;
    }

    maybe_commit($dbh);
  } elsif ($action eq 'kick_machine') {
    die "Must pass machine_id!" if !$machine_id;

    # Check security
    check_edit_machine($login, $dbh, $machine_id, "kick machine");

    my $commands = $dbh->selectrow_arrayref("SELECT commands FROM tbox_machine WHERE machine_id = ?", undef, $machine_id);
    if (!$commands) {
      die "Invalid machine id $machine_id!";
    }
    my @commands = split /,/, $commands->[0];
    if (! grep { $_ eq 'kick' } @commands) {
      push @commands, 'kick';
    }
    my $rows = $dbh->do('UPDATE tbox_machine SET commands = ? WHERE machine_id = ?', undef, join(',', @commands), $machine_id);
    if (!$rows) {
      die "Could not update machine!";
    }
    maybe_commit($dbh);
  } elsif ($action eq 'delete_machine') {
    die "Must pass machine_id!" if !$machine_id;

    # Check security
    check_edit_machine($login, $dbh, $machine_id, "delete machine");

    my $row = $dbh->do('DELETE FROM tbox_build_field WHERE machine_id = ?', undef, $machine_id);
    $row = $dbh->do('DELETE FROM tbox_build_comment WHERE machine_id = ?', undef, $machine_id);
    $row = $dbh->do('DELETE FROM tbox_build WHERE machine_id = ?', undef, $machine_id);
    $row = $dbh->do('DELETE FROM tbox_machine WHERE machine_id = ?', undef, $machine_id);
    die "Could not delete machine" if !$row;
    delete_logs($machine_id);
    maybe_commit($dbh);

  }
  return $machine_id;
}


sub update_bonsai_action {
  my ($p, $dbh, $login) = @_;

  my $tree = $p->param('tree') || "";
  my $bonsai_id = $p->param('bonsai_id') || "";

  my $action = $p->param('action') || "";

  if ($action eq 'edit_bonsai') {
    my $display_name = $p->param('display_name') || "";
    my $bonsai_url = $p->param('bonsai_url') || "";
    my $module = $p->param('module') || "";
    my $branch = $p->param('branch') || "";
    my $directory = $p->param('directory') || "";
    my $cvsroot = $p->param('cvsroot') || "";

    if ($bonsai_id) {
      # Check security
      check_edit_bonsai($login, $dbh, $bonsai_id);

      my $rows = $dbh->do("UPDATE tbox_bonsai SET display_name = ?, bonsai_url = ?, module = ?, branch = ?, directory = ?, cvsroot = ? WHERE bonsai_id = ?", undef, $display_name, $bonsai_url, $module, $branch, $directory, $cvsroot, $bonsai_id);
      if (!$rows) {
        die "Could not update bonsai!";
      }
      Tinderbox3::Bonsai::clear_cache($dbh, $bonsai_id);
    } else {
      # Check security
      check_edit_tree($login, $dbh, $tree, "edit machine");

      $dbh->do("INSERT INTO tbox_bonsai (tree_name, display_name, bonsai_url, module, branch, directory, cvsroot) VALUES (?, ?, ?, ?, ?, ?, ?)", undef, $tree, $display_name, $bonsai_url, $module, $branch, $directory, $cvsroot);
      $bonsai_id = sql_get_last_id($dbh, 'tbox_bonsai_bonsai_id_seq');
    }
    maybe_commit($dbh);
  } elsif ($action eq "delete_bonsai") {
    Tinderbox3::Bonsai::clear_cache($dbh, $bonsai_id);
    my $rows = $dbh->do("DELETE FROM tbox_bonsai WHERE bonsai_id = ?", undef, $bonsai_id);
    if (!$rows) {
      die "Could not delete bonsai!";
    }
    maybe_commit($dbh);
  }

  return ($tree, $bonsai_id);
}

sub sql_current_timestamp {
  if ($dbtype eq "Pg") {
      return "current_timestamp";
  } elsif ($dbtype eq "mysql") {
      return "current_timestamp()";
  }
}

sub sql_get_timestamp {
  my ($arg) = @_;
  if ($dbtype eq "Pg") {
    return "EXTRACT (EPOCH FROM $arg)";
  } elsif ($dbtype eq "mysql") {
    return "unix_timestamp($arg)";
  }
}

sub sql_abstime {
  my ($arg) = @_;
  if ($dbtype eq "Pg") {
    return "abstime($arg + 0)";
  } elsif ($dbtype eq "mysql") {
    return "from_unixtime($arg)";
  }
}

sub sql_get_last_id {
  my ($dbh, $sequence) = @_;
  if ($dbtype eq "Pg") {
    my $row = $dbh->selectrow_arrayref("SELECT currval('$sequence')");
    return $row->[0];
  } elsif ($dbtype eq "mysql") {
    my $row = $dbh->selectrow_arrayref("SELECT last_insert_id()");
    return $row->[0];
  }
}

sub sql_get_bool {
  my ($bool) = @_;
  if ($dbtype eq 'Pg') {
    return $bool ? 'Y' : 'N';
  } elsif ($dbtype eq 'mysql') {
    return $bool ? 1 : 0;
  }
}

1
