#!/usr/bin/perl -wT -I.

use CGI;
use Tinderbox3::DB;
use Tinderbox3::XML;
my $p = new CGI();
my $dbh = get_dbh();

my $patch_id = $p->param('patch_id') || "";
if (!$patch_id) {
  die_xml_error($p, $dbh, "Must specify patch id!");
}


#
# Get data for response
#
my $patch = $dbh->selectrow_arrayref("SELECT patch FROM tbox_patch WHERE patch_id = ?", undef, $patch_id);
if (!defined($patch)) {
  die_xml_error($p, $dbh, "Could not get tree!");
}

print $p->header("text/plain");

print $patch->[0];

$dbh->disconnect;
