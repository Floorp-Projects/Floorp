package Tinderbox3::Header;

use strict;

use CGI::Carp qw(fatalsToBrowser);

require Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(header footer);

sub header {
  my ($p, $title) = @_;
  print $p->header;
  print <<EOM;
<html>
<head>
<title>Tinderbox - $title</title>
<link rel="stylesheet" type="text/css" href="/~jkeiser/tbox3.css">
</head>
<body>
EOM
}

sub footer {
  print <<EOM;
</body>
</html>
EOM
}

1
