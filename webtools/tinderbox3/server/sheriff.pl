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

# For edit_sheriff
my $tree = Tinderbox3::DB::update_tree_action($p, $dbh, $login);
# Get sheriff tree info from DB
my $tree_info = $dbh->selectrow_arrayref("SELECT header, footer, special_message, sheriff, build_engineer, status, statuses, sheriffs FROM tbox_tree WHERE tree_name = ?", undef, $tree);
if (!defined($tree_info)) {
  die "Could not get tree!";
}

my ($header, $footer, $special_message, $sheriff, $build_engineer, $status, $statuses, $sheriffs) = @{$tree_info};

header($p, $login, $cookie, "Sheriff $tree", $tree);

#
# Edit patch form
#

print <<EOM;
<form name=editform method=post action='sheriff.pl'>
<input type=hidden name=action value='edit_sheriff'>
@{[$p->hidden(-name=>'tree', -default=>$tree)]}
<strong>Status:</strong> @{[$p->popup_menu(-name=>'status', -values=>[split /,/, $statuses], -default=>$status)]}<br>
<strong>Special Message:</strong><br>
@{[$p->textarea(-name=>'special_message', -default=>$special_message,
                -rows=>5, -columns=>100)]}<br>
<strong>Sheriff:</strong><br>
@{[$p->textfield(-name=>'sheriff', -default=>$sheriff, -size=>100)]}<br>
<strong>Build Engineer</strong><br>
@{[$p->textfield(-name=>'build_engineer', -default=>$build_engineer,
                 -size=>100)]}<br>
<strong>Sheriff Privileges (comma separated list of emails):</strong><br>
@{[$p->textfield(-name=>'sheriffs', -default=>$sheriffs, -size=>100)]}<br>
<strong>Header:</strong><br>
@{[$p->textarea(-name=>'header', -default=>$header, -rows=>15, -columns=>100)]}<br>
<strong>Footer</strong><br>
@{[$p->textarea(-name=>'footer', -default=>$footer, -rows=>5, -columns=>100)]}<br>
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
