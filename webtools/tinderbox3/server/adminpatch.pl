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

# For edit_patch
my $patch_id = Tinderbox3::DB::update_patch_action($p, $dbh, $login);
# Get patch from DB
my $patch_info = $dbh->selectrow_arrayref("SELECT tree_name, patch_name, patch_ref, patch, in_use FROM tbox_patch WHERE patch_id = ?", undef, $patch_id);
if (!defined($patch_info)) {
  die "Could not get patch!";
}
my ($tree, $patch_name, $patch_ref, $patch, $in_use) = @{$patch_info};
my $bug_id;
if ($patch_ref =~ /Bug\s+(.*)/) {
  $bug_id = $1;
}

header($p, $login, $cookie, "Edit Patch $patch_name", $tree);

#
# Edit patch form
#

print <<EOM;
<form name=editform method=get action='adminpatch.pl'>
<input type=hidden name=action value='edit_patch'>
<input type=hidden name=patch_id value='$patch_id'>
<table>
<tr><th>Patch Name (just for display):</th><td>@{[$p->textfield(-name=>'patch_name', -default=>$patch_name)]}</td></tr>
<tr><th>Bug #:</th><td>@{[$p->textfield(-name=>'bug_id', -default=>$bug_id)]}</td></tr>
<tr><th>In Use:</th><td><input type=checkbox name=in_use@{[$in_use ? ' checked' : '']}></td></tr>
</table>
EOM

if (!$login) {
  print login_fields();
}

print <<EOM;
<input type=submit>
</form>
<hr>
<PRE>
$patch
</PRE>
EOM


footer($p);
$dbh->disconnect;
