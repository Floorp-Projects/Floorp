#!/usr/bin/perl
# -*- Mode: perl; indent-tabs-mode: nil -*-


#
# Utils used by tests to send data to graph server, or tinderbox.
#

require 5.003;

use strict;

$::Version = '$Revision: 1.3 $ ';

package ReportData;

# Send data to graph cgi via HTTP.
sub send_results_to_server {
    my ($server, $value, $data, $testname, $tbox) = @_;

    my $tmpurl = "http://$server/graph/collect.cgi";
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

# Fake a tinderbox message.
sub send_tbox_packet {
  my ($server_email, $tree, $status,
      $log, $machine, $build_name) = @_;

  my $foo;

  my $now = time();

  $foo .= <<END_PRINT;
tinderbox: tree: $tree
tinderbox: builddate: $now
tinderbox: status: $status
tinderbox: build: $build_name
tinderbox: errorparser: unix
tinderbox: buildfamily: unix
tinderbox: END
END_PRINT

$foo .= $log;

  print "foo = \n$foo\n";

  open MSG, ">msg.$machine";
  print MSG $foo;
  close MSG;

  system "/bin/mail  $server_email " . "< msg.$machine";


  unlink "msg.$machine";
}
