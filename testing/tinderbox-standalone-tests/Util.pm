# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

package Util;

use Sys::Hostname;
use File::Copy;
use POSIX qw(sys_wait_h strftime);

sub print_log {
    my ($text) = @_;
    #print LOG $text;
    print $text;
}

sub print_logfile {
    my ($logfile, $test_name) = @_;
    print "DEBUG: $logfile\n";
    print_log "----------- Output from $test_name ------------- \n";
    open READRUNLOG, "$logfile" or die "Can't open log $logfile: $!\n";
    print_log "  $_" while <READRUNLOG>;
    close READRUNLOG or die "Can't close log $logfile: $!\n";
    print_log "----------- End Output from $test_name --------- \n";
}

sub print_test_errors {
    my ($result, $name) = @_;

    if (not $result->{timed_out} and $result->{exit_value} != 0) {
        if ($result->{sig_name} ne '') {
            print_log "Error: $name: received SIG$result->{sig_name}\n";
        }
        print_log "Error: $name: exited with status $result->{exit_value}\n";
        if ($result->{dumped_core}) {
            print_log "Error: $name: dumped core.\n";
        }
    }
}

# Parse a file for $token, return the token.
# Look for the line "<token><delimiter><return-value>", e.g.
# for "__startuptime,5501"
#   token        = "__startuptime"
#   delimiter    = ","
#   return-value = "5501";
#
sub extract_token {
    my ($output, $token, $delimiter) = @_;
    use Data::Dumper;
    print Dumper("extract_token: @_");
    my $token_value = 0;
    if ($output =~ /$token/) {
        $token_value = substr($output, index($output, $delimiter) + 1);
        chomp($token_value);
    }
    return $token_value;
}

sub run_cmd {
    my ($home_dir, $binary_dir, $args, $timeout_secs) = @_;
    my $now = localtime();
    my $pid = 0;
    my $shell_command = join(' ', @{$args});

    my $exit_value = 1;
    my $signal_num;
    my $sig_name;
    my $dumped_core;
    my $timed_out;
    my $output;

    print_log "Begin: $now\n";
    print_log "cmd = $shell_command\n";

    eval{
      # Set XRE_NO_WINDOWS_CRASH_DIALOG to disable showing
      # the windows crash dialog in case the child process
      # crashes
      $ENV{XRE_NO_WINDOWS_CRASH_DIALOG} = 1;

      # Now cd to dir where binary is..
      chdir $binary_dir or die "chdir($binary_dir): $!\n";
        
      local $SIG{ALRM} = sub { die "alarm" };
      alarm $timeout_secs;
      $pid = open CMD, "$shell_command |"
             or die "Could not run command: $!"; 

      while (<CMD>) {
        $output .= $_;
        print_log $_;
      }
      close CMD or die "Could not close command: $!";
      $exit_value = $? >> 8;
      $signal_num = $? >> 127;
      $sig_name = signal_name($signal_num);
      $dumped_core = $? & 128;
      $timed_out = 0;
      alarm 0;
    };
    if($@){
      if($@ =~ /alarm/){
        $timed_out = 1;
        kill_process($pid);
      }else{
        print_log("Error running $shell_command: $@\n");
        $output = $@;
      }
    }

    $now = localtime();
    print_log "End:   $now\n";

    if ($exit_value || $timed_out || $dumped_core || $signal_num){
      print_log("Error running $shell_command\n");
      if($output){
        print_log("Output: $output\n");
      }
      if ($exit_value) {
        print_log("Exit value: $exit_value\n");
      }
      if ($timed_out) {
          print_log("Timed out\n");
          # callers expect exit_value to be non-zero if request timed out
          $exit_value = 1;
      }
      if ($dumped_core) {
          print_log("Segfault (core dumped)\n");
      }
      if ($signal_num) {
          print_log("Received signal: $sig_name\n");
      }
    }

    return { timed_out=>$timed_out,
             exit_value=>$exit_value,
             sig_name=>$sig_name,
             output=>$output,
             dumped_core=>$dumped_core };
}


sub get_system_cwd {
    my $a = Cwd::getcwd()||`pwd`;
    chomp($a);
    return $a;
}

sub get_graph_tbox_name {
  if ($Settings::GraphNameOverride ne '') {
    return $Settings::GraphNameOverride;
  }

  my $name = hostname();
  if ($Settings::BuildTag ne '') {
    $name .= '_' . $Settings::BuildTag;
  }
  return $name;
}

sub print_log_test_result {
  my ($test_name, $test_title, $num_result, $units, $print_name, $print_result) = @_;

  print_log "\nTinderboxPrint:";
  if ($Settings::TestsPhoneHome) {
    my $time = POSIX::strftime "%Y:%m:%d:%H:%M:%S", localtime;
    print_log "<a title=\"$test_title\" href=\"http://$Settings::results_server/graph/query.cgi?testname=" . $test_name . "&units=$units&tbox=" . get_graph_tbox_name() . "&autoscale=1&days=7&avg=1&showpoint=$time,$num_result\">";
  } else {
    print_log "<abbr title=\"$test_title\">";
  }
  print_log $print_name;
  if (!$Settings::TestsPhoneHome) {
    print_log "</abbr>";
  }
  print_log ':' . $print_result;
  if ($Settings::TestsPhoneHome) {
    print_log "</a>";
  }
  print_log "\n";

}

sub print_log_test_result_ms {
  my ($test_name, $test_title, $result, $print_name) = @_;
  print_log_test_result($test_name, $test_title, $result, 'ms',
                        $print_name, $result . 'ms');
}

sub print_log_test_result_bytes {
  my ($test_name, $test_title, $result, $print_name, $sig_figs) = @_;

  print_log_test_result($test_name, $test_title, $result, 'bytes',
                        $print_name, PrintSize($result, $sig_figs) . 'B');
}

sub print_log_test_result_count {
  my ($test_name, $test_title, $result, $print_name, $sig_figs) = @_;
  print_log_test_result($test_name, $test_title, $result, 'count',
                        $print_name, PrintSize($result, $sig_figs));
}


# Report test results back to a server.
# Netscape-internal now, will push to mozilla.org, ask
# mcafee or jrgm for details.
#
# Needs the following perl stubs, installed for rh7.1:
# perl-Digest-MD5-2.13-1.i386.rpm
# perl-MIME-Base64-2.12-6.i386.rpm
# perl-libnet-1.0703-6.noarch.rpm
# perl-HTML-Tagset-3.03-3.i386.rpm
# perl-HTML-Parser-3.25-2.i386.rpm
# perl-URI-1.12-5.noarch.rpm
# perl-libwww-perl-5.53-3.noarch.rpm
#
sub send_results_to_server {
    my ($value, $raw_data, $testname) = @_;

    # Prepend raw data with cvs checkout date, performance
    # Use MOZ_CO_DATE, but with same graph/collect.cgi format. (server)
    #my $data_plus_co_time = "MOZ_CO_DATE=$co_time_str\t$raw_data";
    my $data_plus_co_time = "MOZ_CO_DATE=test";
    my $tbox = get_graph_tbox_name();

    my $tmpurl = "http://$Settings::results_server/graph/collect.cgi";
    $tmpurl .= "?value=$value&data=$data_plus_co_time&testname=$testname&tbox=$tbox";

    print_log "send_results_to_server(): \n";
    print_log "tmpurl = $tmpurl\n";

    # libwww-perl has process control problems on windows,
    # spawn wget instead.
    if ($Settings::OS =~ /^WIN/) {
        system ("wget", "-O", "/dev/null", $tmpurl);
        print_log "send_results_to_server() succeeded.\n";
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
            print_log "send_results_to_server() failed.\n";
        } else {
            print_log "Results submitted to server: \n" .
              $res->status_line . "\n" . $res->content . "\n";
            print_log "send_results_to_server() succeeded.\n";
        }
    }
}

sub kill_process {
    my ($target_pid) = @_;
    my $start_time = time;

    # Try to kill and wait 10 seconds, then try a kill -9
    my $sig;
    for $sig ('TERM', 'KILL') {
        print "kill $sig $target_pid\n";
        kill $sig => $target_pid;
        my $interval_start = time;
        while (time - $interval_start < 10) {
            # the following will work with 'cygwin' perl on win32, but not 
            # with 'MSWin32' (ActiveState) perl
            my $pid = waitpid($target_pid, POSIX::WNOHANG());
            if (($pid == $target_pid and POSIX::WIFEXITED($?)) or $pid == -1) {
                my $secs = time - $start_time;
                $secs = $secs == 1 ? '1 second' : "$secs seconds";
                print_log "Process killed. Took $secs to die.\n";
                return;
            }
            sleep 1;
        }
    }
    die "Unable to kill process: $target_pid";
}

BEGIN {
    my %sig_num = ();
    my @sig_name = ();

    sub signal_name {
        # Find the name of a signal number
        my ($number) = @_;

        unless (@sig_name) {
            unless($Config::Config{sig_name} && $Config::Config{sig_num}) {
                die "No sigs?";
            } else {
                my @names = split ' ', $Config::Config{sig_name};
                @sig_num{@names} = split ' ', $Config::Config{sig_num};
                foreach (@names) {
                    $sig_name[$sig_num{$_}] ||= $_;
                }
            }
        }
        return $sig_name[$number];
    }
}

sub PercentChange($$) {
    my ($old, $new) = @_;
    if ($old == 0) {
        return 0;
    }
    return ($new - $old) / $old;
}

# Print a value of bytes out in a reasonable
# KB, MB, or GB form.  Sig figs should probably
# be 3, 4, or 5 for most purposes here.  This used
# to default to 3 sig figs, but I wanted 4 so I
# generalized here.  -mcafee
#
# Usage: PrintSize(valueAsInteger, numSigFigs)
# 
sub PrintSize($$) {

    # print a number with 3 significant figures
    sub PrintNum($$) {
        my ($num, $sigs) = @_;
        my $rv;

        # Figure out how many decimal places to show.
        # Only doing a few cases here, for normal range
        # of test numbers.

        # Handle zero case first.
        if ($num == 0) {
          $rv = "0";
        } elsif ($num < 10**($sigs-5)) {
          $rv = sprintf "%.5f", ($num);
        } elsif ($num < 10**($sigs-4)) {
          $rv = sprintf "%.4f", ($num);
        } elsif ($num < 10**($sigs-3)) {
          $rv = sprintf "%.3f", ($num);
        } elsif ($num < 10**($sigs-2)) {
          $rv = sprintf "%.2f", ($num);
        } elsif ($num < 10**($sigs-1)) {
          $rv = sprintf "%.1f", ($num);
        } else {
          $rv = sprintf "%d", ($num);
        }

    }

    my ($size, $sigfigs) = @_;

    # 1K = 1024, previously this was approximated as 1000.
    my $rv;
    if ($size > 1073741824) {  # 1024^3
        $rv = PrintNum($size / 1073741824.0, $sigfigs) . "G";
    } elsif ($size > 1048576) {  # 1024^2
        $rv = PrintNum($size / 1048576.0, $sigfigs) . "M";
    } elsif ($size > 1024) {
        $rv = PrintNum($size / 1024.0, $sigfigs) . "K";
    } else {
        $rv = PrintNum($size, $sigfigs);
    }
}

# Page loader (-f option):
# If you are building optimized, you need to add
#   --enable-trace-malloc --enable-perf-metrics
# to turn the pageloader code on.  If you are building debug you only
# need
#   --enable-trace-malloc
#

sub ReadLeakstatsLog($) {
    my ($filename) = @_;
    my $leaks = 0;
    my $leaked_allocs = 0;
    my $mhs = 0;
    my $bytes = 0;
    my $allocs = 0;

    open LEAKSTATS, "$filename"
        or die "unable to open $filename";
    while (<LEAKSTATS>) {
        chop;
        my $line = $_;
        if ($line =~ /Leaks: (\d+) bytes, (\d+) allocations/) {
            $leaks = $1;
            $leaked_allocs = $2;
        } elsif ($line =~ /Maximum Heap Size: (\d+) bytes/) {
            $mhs = $1;
        } elsif ($line =~ /(\d+) bytes were allocated in (\d+) allocations./) {
            $bytes = $1;
            $allocs = $2;
        }
    }

    return {
        'leaks' => $leaks,
        'leaked_allocs' => $leaked_allocs,
        'mhs' => $mhs,
        'bytes' => $bytes,
        'allocs' => $allocs
        };
}

1;
