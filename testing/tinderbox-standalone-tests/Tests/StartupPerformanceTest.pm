# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Startup performance test.  Time how fast it takes the browser
# to start up.  Some help from John Morrison to get this going.
#
# Needs user_pref("browser.dom.window.dump.enabled", true);
# (or CPPFLAGS=-DMOZ_ENABLE_JS_DUMP in mozconfig since we
# don't have profiles for tbox right now.)
#
# $startup_url needs ?begin=<time> dynamically inserted.
#
sub StartupPerformanceTest {
  my ($test_name, $binary, $build_dir, $startup_test_args, $startup_url) = @_;
  
  my $i;
  my $startuptime;         # Startup time in ms.
  my $agg_startuptime = 0; # Aggregate startup time.
  my $startup_count   = 0; # Number of successful runs.
  my $avg_startuptime = 0; # Average startup time.
  my @times;
  my $startup_test_result = 'success';
  
  for($i=0; $i<10; $i++) {
    # Settle OS.
    system('/bin/sh -c "sync; sleep 5"');
    
    # Generate URL of form file:///<path>/startup-test.html?begin=986869495000
    # Where begin value is current time.
    my ($time, $url, $cwd);
    
    #
    # Test for Time::HiRes and report the time.
    $time = Time::PossiblyHiRes::getTime();
    
    $cwd = get_system_cwd();
    print "cwd = $cwd\n";
    $url  = "$startup_url?begin=$time";
    
    print "url = $url\n";
    
    # Then load startup-test.html, which will pull off the begin argument
    # and compare it to the current time to compute startup time.
    # Since we are iterating here, save off logs as StartupPerformanceTest-0,1,2...
    #
    # -P $Settings::MozProfileName added 3% to startup time, assume one profile
    # and get the 3% back. (http://bugzilla.mozilla.org/show_bug.cgi?id=112767)
    #
    if($startup_test_result eq 'success') {
      $startuptime =
        AliveTestReturnToken("StartupPerformanceTest-$i",
                             $build_dir,
                             [$binary, @$startup_test_args, $url],
                             $Settings::StartupPerformanceTestTimeout,
                             "__startuptime",
                             ",");
    } else {
      print "Startup test failed.\n";
    }
    
    if($startuptime) {
      $startup_test_result = 'success';
      
      # Add our test to the total.
      $startup_count++;
      $agg_startuptime += $startuptime;
      
      # Keep track of the results in an array.
      push(@times, $startuptime);
    } else {
      $startup_test_result = 'testfailed';
      Util::print_log("StartupPerformanceTest: test failed\n");
    }
    
  } # for loop
  
  if($startup_test_result eq 'success') {
    Util::print_log("\nSummary for startup test:\n");
    
    # Print startup times.
    chomp(@times);
    my $times_string = join(" ", @times);
    Util::print_log("times = [$times_string]\n");
    
    # Figure out the average startup time.
    $avg_startuptime = $agg_startuptime / $startup_count;
    Util::print_log("Average startup time: $avg_startuptime\n");
    
    my $min_startuptime = min(@times);
    Util::print_log("Minimum startup time: $min_startuptime\n");
    
    my $ts_prefix = "";
    if($Settings::BinaryName eq "TestGtkEmbed") {
      $ts_prefix = "m";
    }

    Util::print_log_test_result_ms('startup', 'Best startup time out of 10 startups',
                             $min_startuptime, $ts_prefix . 'Ts');
    
    # Report data back to server
    if($Settings::TestsPhoneHome) {
      Util::print_log("phonehome = 1\n");
      Util::send_results_to_server($min_startuptime, $times_string, "startup");
    } 
  }

  return $startup_test_result;
}

1;
