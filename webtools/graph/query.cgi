#!/usr/bin/perl
use CGI::Carp qw(fatalsToBrowser);
use CGI::Request;
use Date::Calc qw(Add_Delta_Days);  # http://www.engelschall.com/u/sb/download/Date-Calc/

my $req = new CGI::Request;

my $TESTNAME  = lc($req->param('testname'));
my $UNITS     = lc($req->param('units'));
my $TBOX      = lc($req->param('tbox'));
my $AUTOSCALE = lc($req->param('autoscale'));
my $DAYS      = lc($req->param('days'));

sub make_filenames_list {
  my ($dir) = @_;

  my @result;

  if (-d "$dir") {
	chdir "$dir";
	while(<*>) {
	  if( $_ ne 'config.txt' ) {
		push @result, $_;
	  }
	}
	chdir "../..";
  }
  return @result;
}

# Print out a list of testnames in db directory
sub print_testnames {
  my ($testname) = @_;

  # HTTP header
  print "Content-type: text/html\n\n<HTML>\n";
  print "<title>testnames</title>";
  print "<center><h2><b>testnames</b></h2></center>";
  print "<p><table width=\"100%\">";
  print "<tr><td align=center>Select one of the following tests:</td></tr>";
  print "<tr><td align=center>\n";
  print " <table><tr><td><ul>\n";

  my @machines = make_filenames_list("db");
  my $machines_string = join(" ", @machines);

  foreach (@machines) {
	print "<li><a href=graph.cgi?&testname=$_$testname>$_</a>\n";
  }
  print "</ul></td></tr></table></td></tr></table>";

}


# Print out a list of machines in db/<testname> directory, with links.
sub print_machines {
  my ($testname) = @_;

  # HTTP header
  print "Content-type: text/html\n\n<HTML>\n";
  print "<title>$TESTNAME machines</title>";
  print "<center><h2><b>$TESTNAME machines:</b></h2></center>";
  print "<p><table width=\"100%\">";
  print "<tr><td align=center>Select one of the following machines:</td></tr>";
  print "<tr><td align=center>\n";
  print " <table><tr><td><ul>\n";

  my @machines = make_filenames_list("db/$testname");
  my $machines_string = join(" ", @machines);

  foreach (@machines) {
	print "<li><a href=graph.cgi?tbox=$_&testname=$testname&autoscale=0&days=0>$_</a>\n";
  }
  print "</ul></td></tr></table></td></tr></table>";

}

sub show_graph {
  # HTTP header
  print "Content-type: text/html\n\n<HTML>\n";
  
  print "<title>$TBOX $TESTNAME</title><br>\n";

  my $neg_autoscale = !$AUTOSCALE;
  print "<a href=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$neg_autoscale&days=$DAYS&units=$UNITS\">autoscale</a><br>\n";


  # graph
  print "<img src=\"graph.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$AUTOSCALE&days=$DAYS&units=$UNITS\">";
}

if(!$TESTNAME) {
  print_testnames();
} elsif(!$TBOX) {
  print_machines($TESTNAME);
} else {
  show_graph();
}


exit 0;

