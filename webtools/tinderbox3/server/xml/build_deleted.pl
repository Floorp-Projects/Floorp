#!/usr/bin/perl -wT -I..

use strict;
use CGI;
use Tinderbox3::DB;
use Tinderbox3::XML;
use Tinderbox3::Log;


our $p = new CGI();
our $dbh = get_dbh();

$SIG{__DIE__} = sub { die_xml_error($p, $dbh, $_[0]); };

my $url = $p->param('url') || "";
if (!$url) {
  die_xml_error($p, $dbh, "Must specify url!");
}

my $rows = $dbh->do("DELETE FROM tbox_build_field WHERE name = ? AND value = ?", undef, "build_zip", $url);
$dbh->commit;

if ($rows eq "0E0") {
  die_xml_error($p, $dbh, "No rows deleted!");
}


#
# Print response
#
print $p->header("text/xml");
print "<response>\n";
print "<builds_deleted rows='$rows'/>\n";
print "</response>\n";

$dbh->disconnect;
