#!/usr/bin/perl
# -*- Mode: perl; indent-tabs-mode: nil -*-

#
# Graph tree-open status, via tbox graph server.
#


# Location of this file.  Make sure that hand-runs and crontab-runs
# of this script read/write the same data.
my $script_dir = "/builds/tinderbox/mozilla/tools/tinderbox";


# Send data to graph server via HTTP.
require "$script_dir/reportdata.pl";

use Sys::Hostname;  # for ::hostname()

sub PrintUsage {
    die <<END_USAGE
Usage:

  $0 <ip or hostname>

END_USAGE
}


# Return true if httpd server is up.
sub is_http_alive() {
  my $url = "http://$ARGV[0]";

  my $rv = 0; # Assume false.

  my $res = 
    eval q{
           use LWP::UserAgent;
           use HTTP::Request;
           my $ua  = LWP::UserAgent->new;
           $ua->timeout(10); # seconds
           
           my $req = HTTP::Request->new(GET => $url);
           my $res = $ua->request($req);
           return $res;
          };
  if ($@) {
    warn "GET Failed: $@";
    #print "GET Failed.\n";
  } else {
    # Debug.
    #print "GET Succeeded: \n";
    #print "status:\n", $res->status_line, "\n";
    #print "content:\n",  $res->content, "\n";
    #print "GET Succeeded.\n";

    # This will be non-zero if http get went through ok.
    if($res->content) {
      $rv = 1;  
    } else {
      $rv = 0;
    }
  }

  return $rv;
}



# main
{
  my $alive_time = 0;  # Hours http has been up.

  my $timefile = "$script_dir/http_alive_timefile.$ARGV[0]";

  PrintUsage() if $#ARGV == -1;
  
  if (is_http_alive()) {
    print "$ARGV[0] is alive\n";

    #
    # Record alive time.
    #

    # Was zero before, report zero here, write out the
    # time for next cycle.
    if (not (-e "$timefile")) {
      print "no timefile found.\n";
      open TIMEFILE, ">$timefile";
      print TIMEFILE time();
      close TIMEFILE;
    } else {
      # Timefile found, compute difference and report that number.
      print "found timefile: $timefile!\n";     
    
      my $time_http_started = 0;
      my $now = 0;
  
      open TIMEFILE, "$timefile";
      while (<TIMEFILE>) {
        chomp;
        $time_http_started = $_;
      }
      close TIMEFILE;
      print "time_http_started = $time_http_started\n";
      
      $now = time();
      print "now = $now\n";

      # Report time in hours.
      $alive_time = ($now - $time_http_started)/3600;
      print "alive_time (hours) = $alive_time\n";

      # Clamp time to 20 hours so we don't get huge spikes for
      # extended open times (weekends)
      if ($alive_time > 20.0) {
        $alive_time = 20.0;
      }
      
    }
    
  } else {
    print "$ARGV[0] is not responding.\n";

    # Delete timefile if there is one.
    if (-e "$timefile") {
      unlink("$timefile");
    }
  }

  ReportData::send_results_to_server("tegu.mozilla.org", 
                                     "$alive_time",
                                     ::hostname(),
                                     "http_alive", 
                                     "$ARGV[0]")

}
