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
if (!$tree) {
  die "Must specify a tree!";
}

header($p, $login, $cookie, "Upload Patch for $tree", $tree);

#
# Upload patch form
#

print <<EOM;
<form name=editform method=post enctype='multipart/form-data' action='admintree.pl'>
<input type=hidden name=action value='upload_patch'>
@{[$p->hidden(-name=>'tree', -default=>$tree)]}
<table>
<tr><th>Patch Name (just for display):</th><td><input type=text name=patch_name></td></tr>
<tr><th>Bug #:</th><td><input type=text name=bug_id></td></tr>
<tr><th>In Use:</th><td><input type=checkbox checked name=in_use></td></tr>
<tr><th>Patch:</th><td><input type=file name=patch></td></tr>
</table>
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
