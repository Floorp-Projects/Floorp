#!/usr/bin/perl -wT -I.

use strict;
use CGI;
use Tinderbox3::Header;
use Tinderbox3::DB;
use Tinderbox3::Login;

my $p = new CGI;
my $dbh = get_dbh();

my ($login, $cookie) = check_session($p, $dbh);
header($p, $login, $cookie, "Login");

print "<form action='admin.pl'>\n";
print login_fields();
print "<input type=submit></form>\n";

footer($p);
$dbh->disconnect;
