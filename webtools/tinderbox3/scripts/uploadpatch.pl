#!/usr/bin/perl -wT -I.

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
