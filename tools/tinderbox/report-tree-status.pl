#!/usr/bin/perl
# -*- Mode: perl; indent-tabs-mode: nil -*-

#
# Graph tree-open status, via tbox graph server.
#


use Sys::Hostname;  # for ::hostname()


# Send data to graph server.
sub send_results_to_server {
    my ($value, $data, $testname, $tbox) = @_;

    my $tmpurl = "http://tegu.mozilla.org/graph/collect.cgi";
    $tmpurl .= "?value=$value&data=$data&testname=$testname&tbox=$tbox";

    print "send_results_to_server(): \n";
    print "tmpurl = $tmpurl\n";

    # libwww-perl has process control problems on windows,
    # spawn wget instead.
    if ($Settings::OS =~ /^WIN/) {
        system ("wget", "-o", "/dev/null", $tmpurl);
        print "send_results_to_server() succeeded.\n";
    } else {
        my $res = eval q{
            use LWP::UserAgent;
            use HTTP::Request;
            my $ua  = LWP::UserAgent->new;
            $ua->timeout(10); # seconds
            my $req = HTTP::Request->new(GET => $tmpurl);
            my $res = $ua->request($req);
            return $res;
        };
        if ($@) {
            warn "Failed to submit startup results: $@";
            print "send_results_to_server() failed.\n";
        } else {
            print "Results submitted to server: \n",
            $res->status_line, "\n", $res->content, "\n";
            print "send_results_to_server() succeeded.\n";
        }
    }
}


sub is_tree_open {
  my $tbox_url = "http://tinderbox.mozilla.org/showbuilds.cgi?tree=SeaMonkey";

  # Dump tbox page source into a file.
  print "HTTP...";
  system ("wget", "-q", "-O", "tbox.source", $tbox_url);
  print "done\n";

  my $rv = 0;

  # Scan file, looking for line that starts with <a NAME="open">
  open TBOX_FILE, "tbox.source";
  while (<TBOX_FILE>) {
    if(/^<a NAME="open">/) {
      # look for "open" string
      if (/open<\/font>$/) {
        print "open\n";
        $rv = 1;
      } else {
        print "closed\n";
        $rv = 0;
      }
    }
  }
  close TBOX_FILE;

  # Clean up.
  unlink("tbox.source");

  return $rv;
}


# main
{
  my $time_since_open = 0;

  # Get tree status.
  if(is_tree_open()) {

    # Record tree open time if not set.
    if (not (-e "timefile")) {
      open TIMEFILE, ">timefile";
      print TIMEFILE time();
      close TIMEFILE;
    } else {
      # Timefile found, compute difference and report that number.
      print "found timefile!\n";     
      
      my $time_tree_opened = 0;
      my $now = 0;
  
      open TIMEFILE, "timefile";
      while (<TIMEFILE>) {
        chomp;
        $time_tree_opened = $_;
      }
      close TIMEFILE;
      print "time_tree_opened = $time_tree_opened\n";
      
      $now = time();
      print "now = $now\n";

      # Debug: PRINT time since open in seconds.
      my $tso_sec =  $now - $time_tree_opened;
      print "time_since_open (sec) = $tso_sec\n";

      # Report time in minutes.  Drop decimal and only use integer.
      $time_since_open = (($now - $time_tree_opened) - ($now - $time_tree_opened)%60)/60;
      print "time_since_open (min) = $time_since_open\n";
    }
    
  } else {
    # tree is closed, leave tree_open_time at zero.

    # Delete timefile if there is one.
    if (-e "timefile") {
      unlink("timefile");
    }
  }


  send_results_to_server("$time_since_open", "--", "treeopen", ::hostname());
}
