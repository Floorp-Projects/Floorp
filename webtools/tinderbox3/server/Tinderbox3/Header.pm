package Tinderbox3::Header;

use strict;

use CGI::Carp qw(fatalsToBrowser);

require Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(header footer);

sub header {
  my ($p, $login, $cookie, $title, $tree, $machine_id, $machine_name) = @_;
  print $p->header(-cookie => $cookie);
  print <<EOM;
<html>
<head>
<title>Tinderbox - $title</title>
<link rel="stylesheet" type="text/css" href="tbox3-admin.css">
</head>
<body>
<h2>$title</h2>
EOM

  nav_links($login, $tree, $machine_id, $machine_name);
}

sub footer {
  print <<EOM;
</body>
</html>
EOM
}

sub nav_links {
  my ($login, $tree, $machine_id, $machine_name) = @_;
  print "<p>\n";
  if ($login) {
    print "Logged in as: $login (<a href='admin.pl?-logout=1'>Log out</a>)\n";
  } else {
    print "Not Logged In (<a href='login.pl'>Log in</a>)\n";
  }
  print "<br><strong><a href='admin.pl'>Administrate Tinderbox</a></strong>";
  if ($tree) {
    print "<br>Tree $tree: <a href='sheriff.pl?tree=$tree'>Sheriff Tree</a> | <a href='admintree.pl?tree=$tree'>Edit Tree</a> | <a href='showbuilds.pl?tree=$tree'>View Tree</a>\n";
    if ($machine_id) {
      print "<br>$machine_name: <a href='adminmachine.pl?machine_id=$machine_id'>Edit</a> | <a href='adminmachine.pl?action=kick_machine&machine_id=$machine_id'>Kick</a>\n";
    }
  }
  print "</strong></p>\n";
}

1
