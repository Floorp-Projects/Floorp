package Tinderbox3::XML;

use strict;
use CGI;

require Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(die_xml_error);

sub die_xml_error {
  my ($p, $dbh, $error) = @_;
  print $p->header("text/xml");
  print "<error>", $p->escapeHTML($error), "</error>\n";
  $dbh->disconnect;
  exit(0);
}

1
