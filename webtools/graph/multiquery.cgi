#!/usr/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Mozilla graph tool.
#
# The Initial Developer of the Original Code is AOL Time Warner, Inc.
#
# Portions created by AOL Time Warner, Inc. are Copyright (C) 2001
# AOL Time Warner, Inc.  All Rights Reserved.
#
# Contributor(s):
#   Chris McAfee <mcafee@mocha.com>
#   John Morrison <jrgm@liveops.com>
#

use CGI::Carp qw(fatalsToBrowser);
use CGI::Request;

my $req = new CGI::Request;

my $TESTNAME  = lc($req->param('testname'));
my $UNITS     = lc($req->param('units'));
my $TBOXES    = lc($req->param('tboxes'));
my $AUTOSCALE = lc($req->param('autoscale'));
my $DAYS      = lc($req->param('days'));
my $LTYPE     = lc($req->param('ltype'));
my $POINTS    = lc($req->param('points'));
my $AVG       = lc($req->param('avg'));

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

  # HTTP header
  print "Content-type: text/html\n\n<HTML>\n";

  print "<title>multiquery: testnames</title>";
  print "<center><h2><b>multiquery: testnames</b></h2></center>";

  print "<p><table width=\"100%\">";
  print "<tr><td align=center>Select one of the following tests:</td></tr>";
  print "<tr><td align=center>\n";
  print " <table><tr><td><ul>\n";

  my @machines = make_filenames_list("db");
  my $machines_string = join(" ", @machines);

  foreach (@machines) {
	print "<li><a href=multiquery.cgi?&testname=$_>$_</a>\n";
  }
  print "</ul></td></tr></table></td></tr></table>";
}

# Print out a list of machines in db/<testname> directory, with links.
sub print_machines {
  my ($testname) = @_;

  # HTTP header
  print "Content-type: text/html\n\n<HTML>\n";
  print "<title>$TESTNAME machines</title>";

  # XXX Testing, this isn't correct yet.
  print "<script>\n";
  print "function scrape_for_names() {\n";
  print "window.location = \"http://www.mocha.com\";\n";
  print "}\n";
  print "</script>\n";


  print "<center><h2><b>$TESTNAME machines:</b></h2></center>";
  print "<p><table width=\"100%\">";
  print "<tr><td align=center>Select one of the following machines:</td></tr>";
  print "<tr><td align=center>\n";
  print " <table><tr><td><ul>\n";

  my @machines = make_filenames_list("db/$testname");
  my $machines_string = join(" ", @machines);


  # XXX Testing, this isn't correct yet.
  print "<form action=\"multiquery.cgi?testname=$TESTNAME\">\n";
  print "<input type=hidden name=\"testname\" value=\"$TESTNAME\">";

  @machines = ("comet", "sleestack");
  foreach(@machines) {
    print "<input type=checkbox name= value=$_>$_<br>\n";
  }

  #print "<input type=hidden name=\"tboxes\" value=\"comet,openwound\">";

  #print "<input type=\"submit\" value=\"submit\">\n";
  print "<INPUT TYPE=\"button\" VALUE=\"Submit\" onClick=\"scrape_for_names()\">\n";
  print "</form>\n";



  print "</ul></td></tr></table></td></tr></table>";

}


sub show_graphs {
  # HTTP header
  print "Content-type: text/html\n\n<HTML>\n";
  
  print "<title>multiquery: $TESTNAME</title><br>\n";

  print "<body>\n";

  # JS refresh every 30min
  print "<script>setTimeout('location.reload()',1800000);</script>\n";

  print "<center><h3>$TESTNAME: $TBOXES</h3></center>\n";

  # Options
  print "<table cellspacing=8>\n";
  print "<tr>\n";


  # Scale Y-axis
  print "<td valign=bottom>\n";
  print "<font size=\"-1\">\n";
  if($AUTOSCALE) {
	print "Y-axis: (<b>zoom</b>|";
	print "<a href=\"multiquery.cgi?tboxes=$TBOXES&testname=$TESTNAME&autoscale=0&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS&avg=$AVG\">100%</a>";
	print ") \n";
  } else {
	print "Y-axis: (";
	print "<a href=\"multiquery.cgi?tboxes=$TBOXES&testname=$TESTNAME&autoscale=1&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS&avg=$AVG\">zoom</a>";
	print "|<b>100%</b>) \n";
  }
  print "</font>\n";
  print "</td>\n";


  # Days, Time-axis
  print "<td valign=bottom>\n";
  print "<form method=\"get\" action=\"multiquery.cgi?tboxes=$TBOXES&testname=$TESTNAME&autoscale=$AUTOSCALE&units=$UNITS&points=$POINTS\">\n";
  print "<font size=\"-1\">\n";
  print "<input type=hidden name=\"tboxes\" value=\"$TBOXES\">";
  print "<input type=hidden name=\"testname\" value=\"$TESTNAME\">";
  print "<input type=hidden name=\"points\" value=\"$POINTS\">";
  print "<input type=hidden name=\"autoscale\" value=\"$AUTOSCALE\">";
  print "<input type=hidden name=\"units\" value=\"$UNITS\">";
  print "<input type=hidden name=\"ltype\" value=\"$LTYPE\">";
  print "<input type=hidden name=\"avg\" value=\"$AVG\">";

  print "Days:";
  if($DAYS) {
	print "(<a href=\"multiquery.cgi?tboxes=$TBOXES&testname=$TESTNAME&autoscale=$AUTOSCALE&units=$UNITS&days=0&points=$POINTS&avg=$AVG\">all data</a>|";
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
	print "<a href=\"multiquery.cgi?tboxes=$TBOXES&testname=$TESTNAME&autoscale=$AUTOSCALE&days=$DAYS&units=$UNITS&ltype=lines&points=$POINTS&avg=$AVG\">lines</a>";
	print "|<b>steps</b>";
	print ")";
  } else {
	print "(<b>lines</b>|";
	print "<a href=\"multiquery.cgi?tboxes=$TBOXES&testname=$TESTNAME&autoscale=$AUTOSCALE&days=$DAYS&units=$UNITS&ltype=steps&points=$POINTS&avg=$AVG\">steps</a>";
	print ")";
  }
  print "</font>\n";
  print "</td>\n";

  # Points
  print "<td valign=bottom>\n";
  print "<font size=\"-1\">\n";
  print "Points:";
  if($POINTS) {
	print "(<b>on</b>|";
	print "<a href=\"multiquery.cgi?tboxes=$TBOXES&testname=$TESTNAME&ltype=$LTYPE&days=$DAYS&autoscale=$AUTOSCALE&points=0&units=$UNITS&avg=$AVG\">off</a>";
	print ")\n";
  } else {
	print "(";
	print "<a href=\"multiquery.cgi?tboxes=$TBOXES&testname=$TESTNAME&ltype=$LTYPE&days=$DAYS&autoscale=$AUTOSCALE&points=1&units=$UNITS&avg=$AVG\">on</a>";	
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
	print "<a href=\"multiquery.cgi?tboxes=$TBOXES&testname=$TESTNAME&autoscale=$AUTOSCALE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS&avg=0\">off</a>";
	print ")\n";
  } else {
	print "(";
	print "<a href=\"multiquery.cgi?tboxes=$TBOXES&testname=$TESTNAME&autoscale=$AUTOSCALE&days=$DAYS&units=$UNITS&ltype=$LTYPE&points=$POINTS&avg=1\">on</a>";	
    print "|<b>off</b>)\n";
  }
  print "</font>\n";
  print "</td>\n";



  print "</tr>\n";

  
  print "</table>\n";
  print "<br>\n";


  print "<table cellspacing=8>\n";

  my @tbox_array = split(",", $TBOXES);
  my $i = 0;
  while($i < @tbox_array) {
    print "<tr>\n";
    print "<td>\n";
    print "<a href=\"query.cgi?tbox=$tbox_array[$i]&testname=$TESTNAME&ltype=$LTYPE&autoscale=$AUTOSCALE&days=$DAYS&avg=$AVG&points=$POINTS&units=$UNITS\"><img src=\"graph.cgi?tbox=$tbox_array[$i]&testname=$TESTNAME&ltype=$LTYPE&size=.6&autoscale=$AUTOSCALE&days=$DAYS&avg=$AVG&points=$POINTS&units=$UNITS\" alt=\"$tbox_array[$i] $TESTNAME graph\" border=0></a>";
    print "</td>\n";
    $i++;

    if($i < @tbox_array) {
    print "<td>\n";
    print "<a href=\"query.cgi?tbox=$tbox_array[$i]&testname=$TESTNAME&ltype=$LTYPE&autoscale=$AUTOSCALE&days=$DAYS&avg=$AVG&points=$POINTS&units=$UNITS\"><img src=\"graph.cgi?tbox=$tbox_array[$i]&testname=$TESTNAME&ltype=$LTYPE&size=.6&autoscale=$AUTOSCALE&days=$DAYS&avg=$AVG&points=$POINTS&units=$UNITS\" alt=\"$tbox_array[$i] $TESTNAME graph\" border=0></a>";
    print "</td>\n";
    $i++;

    print "</tr>\n";
    }
  }

  print "</table>\n";

  #
  # Links at the bottom.
  #

  # luna,sleestack,mecca,mocha
  print "<li>\n";
  print "Multiqueries: (<a href=\"multiquery.cgi?&testname=startup&tboxes=comet,luna,sleestack,mecca,facedown,openwound,rheeeet&points=$POINTS&ltype=$LTYPE&autoscale=$AUTOSCALE&days=$DAYS&units=$UNITS&avg=$AVG\">startup</a>, <a href=\"multiquery.cgi?&testname=xulwinopen&tboxes=comet,luna,sleestack,mecca,facedown,openwound,rheeeet&points=$POINTS&ltype=$LTYPE&autoscale=$AUTOSCALE&days=$DAYS&units=$UNITS&avg=$AVG\">xulwinopen</a>, <a href=\"multiquery.cgi?&testname=pageload&tboxes=btek,luna,sleestack,mecca,mocha&points=$POINTS&ltype=$LTYPE&autoscale=$AUTOSCALE&days=$DAYS&units=$UNITS&avg=$AVG\">pageload</a>, <a href=\"http://tegu.mozilla.org/graph/multiquery.cgi\">build your own multiquery</a>)";
  print "</li>\n";

  print "</body>\n";
}

# main
{
  if(!$TESTNAME) {
    print_testnames();
  } elsif(!$TBOXES) {
    print_machines($TESTNAME);
  } else {
    show_graphs();
  }
}


exit 0;

