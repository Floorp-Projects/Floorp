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
my $SHOWPOINT = lc($req->param('showpoint'));
my $AVG       = lc($req->param('avg'));


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
  my ($tbox, $autoscale, $days, $units, $ltype, $points, $avg) = @_;

  # HTTP header
  print "Content-type: text/html\n\n<HTML>\n";

  print "<title>testnames</title>";
  print "<center><h2><b>testnames</b></h2></center>";

  print "<p><table width=\"100%\">";
  print "<tr><td align=center>Select one of the following tests:</td></tr>";
  print "<tr><td align=center>\n";
  print " <table><tr><td><ul>\n";

  my @tests = make_filenames_list("db");
  my $tests_string = join(" ", @tests);

  foreach (@tests) {
        # 10/13/04 cmc: set defaults to showing points and showing averages
	print "<li><a href=query.cgi?&testname=$_$testname&tbox=$tbox&autoscale=$autoscale&size=$SIZE&days=$days&units=$units&ltype=$ltype&points=$points&showpoint=&avg=$avg>$_</a>\n";
  }
  print "</ul></td></tr></table></td></tr></table>";

}


# Print out a list of machines in db/<testname> directory, with links.
sub print_machines {
  my ($testname, $autoscale, $days, $units, $ltype, $points, $avg) = @_;

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
	print "<li><a href=query.cgi?tbox=$_&testname=$testname&autoscale=$autoscale&size=$SIZE&days=$days&units=$units&ltype=$ltype&points=$points&avg=$avg&showpoint=>$_</a>\n";
  }
  print "</ul></td></tr></table></td></tr></table>";

}

sub show_graph {
  # HTTP header
  print "Content-type: text/html\n\n<HTML>\n";
  
  print "<title>$TESTNAME $TBOX</title><br>\n";

  print "<body>\n";

  # JS refresh every 30min
  print "<script>setTimeout('location.reload()',1800000);</script>\n";


  print "<center>\n";
  print "<table cellspacing=8>\n";

  print "<tr>\n";
  print "<b>$TESTNAME</b><br>($TBOX)\n";
  print "</tr>\n";

  print "<tr>\n";

  # Scale Y-axis
  print "<td valign=bottom>\n";
  print "<font size=\"-1\">\n";
  if($AUTOSCALE) {
	print "Y-axis: (<b>zoom</b>|";
	print "<a href=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=0&size=$SIZE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS&showpoint=$SHOWPOINT&avg=$AVG\">100%</a>";
	print ") \n";
  } else {
	print "Y-axis: (";
	print "<a href=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=1&size=$SIZE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS&showpoint=$SHOWPOINT&avg=$AVG\">zoom</a>";
	print "|<b>100%</b>) \n";
  }
  print "</font>\n";
  print "</td>\n";

  # Days, Time-axis
  print "<td valign=bottom>\n";
  print "<form method=\"get\" action=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$AUTOSCALE&size=$SIZE&units=$UNITS&ltype=$LTYPE&points=$POINTS&avg=$AVG\">\n";
  print "<font size=\"-1\">\n";
  print "<input type=hidden name=\"tbox\" value=\"$TBOX\">";
  print "<input type=hidden name=\"testname\" value=\"$TESTNAME\">";
  print "<input type=hidden name=\"autoscale\" value=\"$AUTOSCALE\">";
  print "<input type=hidden name=\"size\" value=\"$SIZE\">";
  print "<input type=hidden name=\"units\" value=\"$UNITS\">";
  print "<input type=hidden name=\"ltype\" value=\"$LTYPE\">";
  print "<input type=hidden name=\"points\" value=\"$POINTS\">";
  print "<input type=hidden name=\"showpoint\" value=\"$SHOWPOINT\">";
  print "<input type=hidden name=\"avg\" value=\"$AVG\">";

  print "Days:";
  if($DAYS) {
	print "(<a href=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$AUTOSCALE&size=$SIZE&days=0&units=$UNITS&ltype=$LTYPE&points=$POINTS&showpoint=$SHOWPOINT&avg=$AVG\">all data</a>|";
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
	print "<a href=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$AUTOSCALE&size=$SIZE&days=$DAYS&units=$UNITS&ltype=lines&points=$POINTS&showpoint=$SHOWPOINT&avg=$AVG\">lines</a>";
	print "|<b>steps</b>";
	print ")";
  } else {
	print "(<b>lines</b>|";
	print "<a href=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$AUTOSCALE&size=$SIZE&days=$DAYS&units=$UNITS&ltype=steps&points=$POINTS&showpoint=$SHOWPOINT&avg=$AVG\">steps</a>";
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
	print "<a href=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$AUTOSCALE&size=$SIZE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=0&showpoint=$SHOWPOINT&avg=$AVG\">off</a>";
	print ")\n";
  } else {
	print "(";
	print "<a href=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$AUTOSCALE&size=$SIZE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=1&showpoint=$SHOWPOINT&avg=$AVG\">on</a>";	
    print "|<b>off</b>)\n";
  }
  print "</font>\n";
  print "</td>\n";


  # Average (on|off)
  print "<td valign=bottom>\n";
  print "<font size=\"-1\">\n";
  print "<A title=\"Moving average of last 10 points\">Average:</a>";
  if($AVG) {
	print "(<b>on</b>|";
	print "<a href=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$AUTOSCALE&size=$SIZE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS&showpoint=$SHOWPOINT&avg=0\">off</a>";
	print ")\n";
  } else {
	print "(";
	print "<a href=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$AUTOSCALE&size=$SIZE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS&showpoint=$SHOWPOINT&avg=1\">on</a>";	
    print "|<b>off</b>)\n";
  }
  print "</font>\n";
  print "</td>\n";


  print "</tr>\n";

  print "</table>\n";
  print "<br>\n";


  # graph
  print "<img src=\"graph.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$AUTOSCALE&size=$SIZE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS&showpoint=$SHOWPOINT&avg=$AVG\" alt=\"$TBOX $TESTNAME graph\">";

  print "<br>\n";
  print "<br>\n";


  print "<table cellspacing=8>\n";
  print "<tr>\n";

  print "<td>\n";

  #
  # Links at the bottom
  #

  # Other machines
  print "<font size=\"-1\">";
  print "<li>\n";
  print "<a href=\"query.cgi?tbox=&testname=$TESTNAME&autoscale=$AUTOSCALE&size=$SIZE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS&avg=$AVG\">Other machines running the $TESTNAME test</a>";
  print "</li>\n";

  # Other tests
  print "<li>\n";
  print "Other $TBOX tests: (<a href=\"query.cgi?tbox=$TBOX&testname=startup&autoscale=$AUTOSCALE&size=$SIZE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS&avg=$AVG\">startup</a>, <a href=\"query.cgi?tbox=$TBOX&testname=xulwinopen&autoscale=$AUTOSCALE&size=$SIZE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS&avg=$AVG\">xulwinopen</a>, <a href=\"query.cgi?tbox=$TBOX&testname=pageload&autoscale=$AUTOSCALE&size=$SIZE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS&avg=$AVG\">pageload</a>, 
<a href=\"query.cgi?tbox=$TBOX&testname=&autoscale=$AUTOSCALE&size=$SIZE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS&avg=$AVG\">show all tests</a>)";
  print "</li>\n";


  # Multiplots, hard-coded for now.
  # luna,sleestack,mecca,mocha
  print "<li>\n";
  print "Multiqueries: (<a href=\"multiquery.cgi?&testname=startup&tboxes=comet,luna,sleestack,mecca,facedown,openwound,rheeeet&days=$DAYS&autoscale=$AUTOSCALE&units=$UNITS&ltype=$LTYPE&points=$POINTS&avg=$AVG\">startup</a>, <a href=\multiquery.cgi?&testname=xulwinopen&tboxes=comet,luna,sleestack,mecca,facedown,openwound,rheeeet&days=$DAYS&autoscale=$AUTOSCALE&units=$UNITS&ltype=$LTYPE&points=$POINTS&avg=$AVG\">xulwinopen</a>, <a href=\"multiquery.cgi?&testname=pageload&tboxes=btek,luna,sleestack,mecca,mocha&days=$DAYS&autoscale=$AUTOSCALE&units=$UNITS&ltype=$LTYPE&points=$POINTS&avg=$AVG\">pageload</a>, <a href=\"multiquery.cgi\">build your own multiquery</a>)";
  print "</li>\n";


  # Raw data.
  print "<li>\n";
  print "<a href=\"rawdata.cgi?tbox=$TBOX&testname=$TESTNAME&days=$DAYS\">Show the raw data for this plot</a>";
  print "</li>\n";

  # Mailto
  print "<li>\n";
  print "<a href=\"mailto:mozilla-performance\@mozilla.org?subject=Perf: $TBOX/$TESTNAME test regression\">Mail m.performance about regressions for this test</a>";
  print "</li>\n";

  # Newsgroup
  print "<li>\n";
  print "<a href=\"news://news.mozilla.org/netscape.public.mozilla.performance\">performance newsgroup</a>";
  print "</li>\n";

  print "</font>";
  
  print "</td>\n";

  print "<td>\n";

  # Graph size
  print "<td valign=\"top\">\n";
  print "<form method=\"get\" action=\"query.cgi?tbox=$TBOX&testname=$TESTNAME&autoscale=$AUTOSCALE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS&avg=$AVG\">\n";
  print "<font size=\"-1\">\n";
  print "<input type=hidden name=\"tbox\" value=\"$TBOX\">";
  print "<input type=hidden name=\"testname\" value=\"$TESTNAME\">";
  print "<input type=hidden name=\"autoscale\" value=\"$AUTOSCALE\">";
  print "<input type=hidden name=\"days\" value=\"$DAYS\">";
  print "<input type=hidden name=\"units\" value=\"$UNITS\">";
  print "<input type=hidden name=\"ltype\" value=\"$LTYPE\">";
  print "<input type=hidden name=\"points\" value=\"$POINTS\">";
  print "<input type=hidden name=\"showpoint\" value=\"$SHOWPOINT\">";
  print "<input type=hidden name=\"avg\" value=\"$AVG\">";

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
  print "</center>\n";

  print "</body>\n";
}

# main
{
  if(!$TESTNAME) {
    print_testnames($TBOX, $AUTOSCALE, $DAYS, $UNITS, $LTYPE, $POINTS, $AVG);
  } elsif(!$TBOX) {
    print_machines($TESTNAME, $AUTOSCALE, $DAYS, $UNITS, $LTYPE, $POINTS, $AVG);
  } else {
    show_graph();
  }
}


exit 0;

