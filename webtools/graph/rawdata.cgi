#!/usr/bin/perl
# -*- Mode: perl; indent-tabs-mode: nil -*-
use CGI::Carp qw(fatalsToBrowser);
use CGI::Request;
use Date::Calc qw(Add_Delta_Days);  # http://www.engelschall.com/u/sb/download/Date-Calc/

my $req = new CGI::Request;

my $TESTNAME  = lc($req->param('testname'));
my $TBOX      = lc($req->param('tbox'));
my $DAYS      = lc($req->param('days'));
my $DATAFILE  = "db/$TESTNAME/$TBOX";


sub show_data {
  die "$TBOX: no data file found." 
	unless -e $DATAFILE; 

  #
  # At some point we might want to clip the data printed
  # based on days=n.

  # Set scaling for x-axis (time)
  #my $today;
  #my $n_days_ago;

  # Get current time, $today.
  #my ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdat) = localtime();  
  #$year += 1900;
  #$mon += 1;
  #$today = sprintf "%04d:%02d:%02d:%02d:%02d:%02d", $year, $mon, $mday, $hour, $min, $sec;

  # Calculate date $DAYS before $today.
  #my ($year2, $mon2, $mday2) = Add_Delta_Days($year, $mon, $mday, -$DAYS);
  #$n_days_ago= sprintf "%04d:%02d:%02d:%02d:%02d:%02d", $year2, $mon2, $mday2, $hour, $min, $sec;


  # Print data that got plotted.
  print "<b>Data in plot form: (t, y)</b><br>\n";
  open DATA, "$DATAFILE" or die "Couldn't open file, $DATAFILE: $!";
  while (<DATA>) {
    my @line = split('\t',$_);
    print "@line[0] @line[1]<br>\n";
  }
  close DATA;


  # Print data again, in raw format.
  print "<br>\n";
  print "<br>\n";
  print "<b>Data in raw form: (t, y, opaque data, ip, user agent)</b><br>\n";
  open DATA, "$DATAFILE" or die "Couldn't open file, $DATAFILE: $!";
  while (<DATA>) {
    print "$_<br>\n";
  }
  close DATA;  

}

# main
{
	
  print "Content-type: text/html\n\n<HTML>\n";
  print "<title>$TESTNAME machines</title>";

  if(!$TESTNAME) {
	print "Error: need testname.";
  } elsif(!$TBOX) {
	print "Error: need machine name.";
  } else {
	show_data();
  }
}

exit 0;

