#!/usr/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
use CGI::Carp qw(fatalsToBrowser);
use CGI::Request;

my $req = new CGI::Request;

my $TBOXES   = lc($req->param('tboxes'));
my $TESTNAME = lc($req->param('testname'));

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

  print "<table cellspacing=8>\n";

  my @tbox_array = split(",", $TBOXES);
  my $i = 0;
  while($i < @tbox_array) {
    print "<tr>\n";
    print "<td>\n";
    print "<a href=\"http://tegu.mozilla.org/graph/query.cgi?tbox=$tbox_array[$i]&testname=$TESTNAME&autoscale=1&days=7&avg=1\"><img src=\"http://tegu.mozilla.org/graph/graph.cgi?tbox=$tbox_array[$i]&testname=$TESTNAME&size=.6&autoscale=1&days=7&avg=1\" alt=\"$tbox_array[$i] $TESTNAME graph\" border=0></a>";
    print "</td>\n";
    $i++;

    if($i < @tbox_array) {
    print "<td>\n";
    print "<a href=\"http://tegu.mozilla.org/graph/query.cgi?tbox=$tbox_array[$i]&testname=$TESTNAME&autoscale=1&days=7&avg=1\"><img src=\"http://tegu.mozilla.org/graph/graph.cgi?tbox=$tbox_array[$i]&testname=$TESTNAME&size=.6&autoscale=1&days=7&avg=1\" alt=\"$tbox_array[$i] $TESTNAME graph\" border=0></a>";
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
  print "Multiqueries: (<a href=\"http://tegu.mozilla.org/graph/multiquery.cgi?&testname=startup&tboxes=comet,luna,sleestack,mecca,facedown,openwound,rheeeet\">startup</a>, <a href=\"http://tegu.mozilla.org/graph/multiquery.cgi?&testname=xulwinopen&tboxes=comet,luna,sleestack,mecca,facedown,openwound,rheeeet\">xulwinopen</a>, <a href=\"http://tegu.mozilla.org/graph/multiquery.cgi?&testname=pageload&tboxes=btek,64.236.138.128,64.236.138.100,64.236.139.180,64.236.139.71\">pageload</a>, <a href=\"http://tegu.mozilla.org/graph/multiquery.cgi\">build your own multiquery</a>)";
  print "</li>\n";

  print "<li>\n";
  print "Pageload hack: 64.236.138.128=luna, 64.236.138.100=sleestack, 64.236.139.180=mecca, 64.236.139.71=mocha";
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

