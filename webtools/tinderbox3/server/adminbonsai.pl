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

# For edit_bonsai
my ($tree, $bonsai_id) = Tinderbox3::DB::update_bonsai_action($p, $dbh, $login);
my ($display_name, $bonsai_url, $module, $branch, $directory, $cvsroot);
# Get bonsai info from DB
if ($bonsai_id) {
  my $bonsai_info = $dbh->selectrow_arrayref("SELECT tree_name, display_name, bonsai_url, module, branch, directory, cvsroot FROM tbox_bonsai WHERE bonsai_id = ?", undef, $bonsai_id);
  if (!defined($bonsai_info)) {
    die "Could not get bonsai info!";
  }
  ($tree, $display_name, $bonsai_url, $module, $branch, $directory, $cvsroot) = @{$bonsai_info};
} else {
  ($display_name, $bonsai_url, $module, $branch, $directory, $cvsroot) = (
    $Tinderbox3::InitialValues::display_name,
    $Tinderbox3::InitialValues::bonsai_url,
    $Tinderbox3::InitialValues::module,
    $Tinderbox3::InitialValues::branch,
    $Tinderbox3::InitialValues::directory,
    $Tinderbox3::InitialValues::cvsroot);
}

header($p, $login, $cookie, "Edit Bonsai $display_name", $tree);

#
# Edit bonsai form
#

print <<EOM;
<form name=editform method=get action='adminbonsai.pl'>
<input type=hidden name=action value='edit_bonsai'>
<input type=hidden name=tree value='$tree'>
<input type=hidden name=bonsai_id value='$bonsai_id'>
<table>
<tr><th>Display Name:</th><td>@{[$p->textfield(-name=>'display_name', -default=>$display_name)]}</td></tr>
<tr><th>Bonsai URL:</th><td>@{[$p->textfield(-name=>'bonsai_url', -default=>$bonsai_url)]}</td></tr>
<tr><th>Module:</th><td>@{[$p->textfield(-name=>'module', -default=>$module)]}</td></tr>
<tr><th>Branch:</th><td>@{[$p->textfield(-name=>'branch', -default=>$branch)]}</td></tr>
<tr><th>Directories (comma separated):</th><td>@{[$p->textfield(-name=>'directory', -default=>$directory)]}</td></tr>
<tr><th>cvsroot (hidden parameter in Bonsai):</th><td>@{[$p->textfield(-name=>'cvsroot', -default=>$cvsroot)]}</td></th></tr>
</table>
<b>PLEASE NOTE that pressing submit will clear the Bonsai cache for this tree.  This is not disastrous by any means, but don't submit unless you are actually changing or adding this tree.</p>
EOM

if (!$login) {
  print login_fields();
}

print <<EOM;
<input type=submit>
</form>
EOM


footer($p);
$dbh->disconnect;
