#!/usr/bin/perl
use CGI::Carp qw(fatalsToBrowser);
use CGI::Request;

my $req = new CGI::Request;

my $TBOX     = lc($req->param('tbox'));
my $DATAFILE = "db/$TBOX";

sub make_machine_list {
  my @result;
  chdir "db";
  while(<*>) {
    if( $_ ne 'config.txt' ) {
      push @result, $_;
    }
  }
  chdir "..";
  return @result;
}

# no tbox, print out a list of machines in db directory, with links.
sub print_machines {
  # HTTP header
  print "Content-type: text/html\n\n<HTML>\n";
  print "<center><h2><b>XUL Window Open times:</b></h2></center>";
  print "<p><table width=\"100%\">";
  print "<tr><td align=center>Select one of the following machines:</td></tr>";
  print "<tr><td align=center>\n";
  print " <table><tr><td><ul>\n";

  my @machines = make_machine_list();
  my $machines_string = join(" ", @machines);

  foreach (@machines) {
	print "<li><a href=query.cgi?tbox=$_>$_</a>\n";
  }
  print "</ul></td></tr></table></td></tr></table>";

}

sub show_graph {
  die "$TBOX is not a valid machine name" 
	unless -e $DATAFILE; 

  my $PNGFILE  = "/tmp/gnuplot.$$";

  # Find gnuplot, sorry this is for solaris.
  my $gnuplot;
  if(-f "/usr/bin/gnuplot") {
	$gnuplot = "/usr/bin/gnuplot";
  } elsif(-f "/usr/local/bin/gnuplot") {
	$gnuplot = "/usr/local/bin/gnuplot";
	$ENV{LD_LIBRARY_PATH} .= "/usr/local/lib";
  } else {
	die "Can't find gnuplot.";
  }

  # interpolate params into gnuplot command
  my $cmds = qq{
				reset
				set term png color
				set output "$PNGFILE"
				set title  "$TBOX XUL Open Window Times"
				set key graph 0.1,0.95 reverse spacing .75 width -18
				set linestyle 1 lt 1 lw 1 pt 7 ps 0
				set linestyle 2 lt 1 lw 1 pt 7 ps 1
				set data style points
				set timefmt "%Y:%m:%d:%H:%M:%S"
				set yrange [ 0 : ]
				set xdata time
				set ylabel "XUL Window Open time (msec.)"
				set timestamp "Generated: %d/%b/%y %H:%M" 0,0 
				set format x "%h %d"
				set grid
				plot "$DATAFILE" using 1:2 with points ls 1, "$DATAFILE" using 1:2 with lines ls 2
			   };

  open  (GNUPLOT, "| $gnuplot") || die "can't fork: $!";
  print  GNUPLOT $cmds;
  close (GNUPLOT) || die "can't close: $!";
  open  (GNUPLOT, "< $PNGFILE") || die "can't read: $!";
  { local $/; $blob = <GNUPLOT>; }
  close (GNUPLOT) || die "can't close: $!";
  unlink $PNGFILE || die "can't unlink $PNGFILE: $!";
  
  print "Content-type: image/png\n\n";
  print $blob;
}

# main
{
  unless ($TBOX) {
	print_machines();
  } else {
	show_graph();
  }
}

exit 0;

