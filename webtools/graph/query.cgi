#!/usr/bin/perl
# -*- Mode: perl; indent-tabs-mode: nil -*-
use CGI::Carp qw(fatalsToBrowser);
use CGI::Request;
use Date::Calc qw(Add_Delta_Days);  # http://www.engelschall.com/u/sb/download/Date-Calc/

my $req = new CGI::Request;

my $TESTNAME  = lc($req->param('testname'));
my $UNITS     = lc($req->param('units'));
my $TBOX      = lc($req->param('tbox'));
my $AUTOSCALE = lc($req->param('autoscale'));
my $SIZE      = lc($req->param('size'));
my $DAYS      = lc($req->param('days'));
my $LTYPE     = lc($req->param('ltype'));
my $POINTS    = lc($req->param('points'));
#
# Testing only:
#
#$TESTNAME  = "testname";
#$UNITS     = "units";
#$TBOX      = "tbox";
#$AUTOSCALE = 1;
#$DAYS      = 1;


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
  my ($tbox, $autoscale, $days, $units, $ltype, $points) = @_;

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
	print "<li><a href=query.cgi?&testname=$_$testname&tbox=$tbox&autoscale=$autoscale&size=$SIZE&days=$days&units=$units&ltype=$ltype&points=$points>$_</a>\n";
  }
  print "</ul></td></tr></table></td></tr></table>";

}


# Print out a list of machines in db/<testname> directory, with links.
sub print_machines {
  my ($testname, $autoscale, $days, $units, $ltype, $points) = @_;

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
	print "<li><a href=query.cgi?tbox=$_&testname=$testname&autoscale=$autoscale&size=$SIZE&days=$days&units=$units&ltype=$ltype&points=$points>$_</a>\n";
  }
  print "</ul></td></tr></table></td></tr></table>";

}

sub show_graph {
  # HTTP header
  print "Content-type: text/html\n\n<HTML>\n";
  
  print "<title>$TBOX $TESTNAME</title><br>\n";

  print "<body>\n";

  print "<table cellspacing=8>\n";
  print "<tr>\n";

  # Scale Y-axis
  print "<td valign=bottom>\n";
  print "<font size=\"-1\">\n";
  if($AUTOSCALE) {
	print "Y-axis: (<b>zoom</b>|";
	print "<a href=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=0&size=$SIZE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS\">100%</a>";
	print ") \n";
  } else {
	print "Y-axis: (";
	print "<a href=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=1&size=$SIZE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS\">zoom</a>";
	print "|<b>100%</b>) \n";
  }
  print "</font>\n";
  print "</td>\n";

  # Days, Time-axis
  print "<td valign=bottom>\n";
  print "<form method=\"get\" action=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$AUTOSCALE&size=$SIZE&units=$UNITS&ltype=$LTYPE&points=$POINTS\">\n";
  print "<font size=\"-1\">\n";
  print "<input type=hidden name=\"tbox\" value=\"$TBOX\">";
  print "<input type=hidden name=\"testname\" value=\"$TESTNAME\">";
  print "<input type=hidden name=\"autoscale\" value=\"$AUTOSCALE\">";
  print "<input type=hidden name=\"size\" value=\"$SIZE\">";
  print "<input type=hidden name=\"units\" value=\"$UNITS\">";
  print "<input type=hidden name=\"ltype\" value=\"$LTYPE\">";
  print "<input type=hidden name=\"points\" value=\"$POINTS\">";

  print "Days:";
  if($DAYS) {
	print "(<a href=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$AUTOSCALE&size=$SIZE&days=0&units=$UNITS&ltype=$LTYPE&points=$POINTS\">all data</a>|";
    print "<input type=text value=$DAYS name=\"days\" size=3 maxlength=10>";
	print ")\n";
  } else {
	print "(<b>all data</b>|";
    print "<input type=text value=\"\" name=\"days\" size=3 maxlength=10>";
	print ")\n";
  }
  print "</font>\n";
  print "</form>\n";
  print "</td>\n";

  # Line style (lines|steps)
  print "<td valign=bottom>\n";
  print "<font size=\"-1\">\n";
  print "Style:";
  if($LTYPE eq "steps") {
	print "(";
	print "<a href=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$AUTOSCALE&size=$SIZE&days=$DAYS&units=$UNITS&ltype=lines&points=$POINTS\">lines</a>";
	print "|<b>steps</b>";
	print ")";
  } else {
	print "(<b>lines</b>|";
	print "<a href=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$AUTOSCALE&size=$SIZE&days=$DAYS&units=$UNITS&ltype=steps&points=$POINTS\">steps</a>";
	print ")";
  }
  print "</font>\n";
  print "</td>\n";

  # Points (on|off)
  print "<td valign=bottom>\n";
  print "<font size=\"-1\">\n";
  print "Points:";
  if($POINTS) {
	print "(<b>on</b>|";
	print "<a href=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$AUTOSCALE&size=$SIZE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=0\">off</a>";
	print ")\n";
  } else {
	print "(";
	print "<a href=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$AUTOSCALE&size=$SIZE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=1\">on</a>";	
    print "|<b>off</b>)\n";
  }
  print "</font>\n";
  print "</td>\n";

  print "</tr>\n";
  print "</table>\n";
  print "<br>\n";

  # graph
  print "<img src=\"graph.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$AUTOSCALE&size=$SIZE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS\" alt=\"$TBOX $TESTNAME graph\">";

  print "<br>\n";
  print "<br>\n";


  print "<table cellspacing=8>\n";
  print "<tr>\n";

  print "<td>\n";

  # Other machines
  print "<font size=\"-1\">";
  print "<li>\n";
  print "<a href=\"query.cgi?tbox=&testname=$TESTNAME&autoscale=$AUTOSCALE&size=$SIZE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS\">Other machines running the $TESTNAME test</a>";
  print "</li>\n";

  print "<li>\n";
  print "<a href=\"query.cgi?tbox=$TBOX&testname=&autoscale=$AUTOSCALE&size=$SIZE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS\">Other tests that $TBOX is running</a>";
  print "</li>\n";

  print "<li>\n";
  print "<a href=\"rawdata.cgi?tbox=$TBOX&testname=$TESTNAME&days=$DAYS\">Print the raw data for this plot.</a>";
  print "</li>\n";

  print "</font>";
  
  print "</td>\n";

  print "<td>\n";

  # Graph size
  print "<td valign=\"top\">\n";
  print "<form method=\"get\" action=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$AUTOSCALE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS\">\n";
  print "<font size=\"-1\">\n";
  print "<input type=hidden name=\"tbox\" value=\"$TBOX\">";
  print "<input type=hidden name=\"testname\" value=\"$TESTNAME\">";
  print "<input type=hidden name=\"autoscale\" value=\"$AUTOSCALE\">";
  print "<input type=hidden name=\"days\" value=\"$DAYS\">";
  print "<input type=hidden name=\"units\" value=\"$UNITS\">";
  print "<input type=hidden name=\"ltype\" value=\"$LTYPE\">";
  print "<input type=hidden name=\"points\" value=\"$POINTS\">";

  print "Graph size:";
  if($SIZE) {
    print "<input type=text value=$SIZE name=\"size\" size=3 maxlength=10>\n";
  } else {
    print "<input type=text value=1.0 name=\"size\" size=3 maxlength=10>\n";
  }

  print "</font>\n";
  print "</form>\n";
  print "</td>\n";

  print "</td>\n";

  print "</tr>\n";
  print "</table>\n";

  print "</body>\n";
}

if(!$TESTNAME) {
  print_testnames($TBOX, $AUTOSCALE, $DAYS, $UNITS, $LTYPE, $POINTS);
} elsif(!$TBOX) {
  print_machines($TESTNAME, $AUTOSCALE, $DAYS, $UNITS, $LTYPE, $POINTS);
} else {
  show_graph();
}


exit 0;

