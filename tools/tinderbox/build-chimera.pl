#!/usr/bin/perl
# -*- Mode: perl; indent-tabs-mode: nil -*-
#

#
# This script gets called after a full mozilla build & test.
# Use this to build and test an embedded or commercial branding of Mozilla.
#
# ./build-seamonkey-utils.pl will call PostMozilla::main() after
# a successful build and testing of mozilla.  This package can report
# status via the $TinderUtils::build_status variable.  Yeah this is a hack,
# but it works for now.  Feel free to improve this mechanism to properly
# return values & stuff.  -mcafee
#
# Things to do:
#   * Get pull by branch working
#
#
use strict;
use File::Path;     # for rmtree();

package PostMozilla;

sub checkout {
  my ($mozilla_build_dir) = @_;

  # chdir to build directory
  chdir "$mozilla_build_dir";

  # Next, do the checkout:
  print "Settings::CVS = $Settings::CVS\n";
  my $status = TinderUtils::run_shell_command("$Settings::CVS checkout mozilla/chimera");

  # hack in the chimera prefs, if needed

  return $status;
}


sub main {
  my ($mozilla_build_dir) = @_;

  TinderUtils::print_log "Starting chimera build.\n";

  # Tell chimera to unbuffer stdio, otherwise when we kill the child process
  # stdout & stderr won't get flushed.
  $ENV{MOZ_UNBUFFERED_STDIO} = 1;

  # Pending a config file, stuff things here.
  my $post_status = 'success';  # Success until we report a failure.
  my $status = 0;  # 0 = success

  # Control tests from here, sorry no config file yet.
  my $chimera_alive_test              = 1;
  my $chimera_test8_test              = 1;
  my $chimera_startup_test            = 0;
  my $chimera_layout_performance_test = 0;

  my $chimera_clean_profile = 1;

  # Build flags
  my $chimera_build_static = 0;
  my $chimera_build_opt    = 1;

  my $chimera_dir = "$mozilla_build_dir/mozilla/chimera";
  my $embedding_dir = "$mozilla_build_dir/mozilla/embedding/config";
  my $chimera_binary = "Navigator";

  unless ($Settings::TestOnly) {
    # Checkout/update the chimera code.
    # Chimera branch doing this for us, we will need this later.
    # $status = checkout($mozilla_build_dir);
    # TinderUtils::print_log "Status from checkout: $status\n";
    # if ($status != 0) {
    #   $post_status = 'busted';
    # }
    
    # Build chimera if we passed the checkout command.
    if ($post_status ne 'busted') {

      #
      # Build embedding/config.
      #
      
      chdir $embedding_dir;
      
      if ($status == 0) {
        $status = TinderUtils::run_shell_command("make");
        TinderUtils::print_log "Status from make: $status\n";
      }
      
      #
      # Build chimera.
      #
      
      chdir $chimera_dir;
      
      if ($status == 0) {
        TinderUtils::print_log "Deleting binary...\n";
        TinderUtils::DeleteBinary("$chimera_dir/build/Navigator.app/Contents/MacOS/$chimera_binary");
          
        # Always do a clean build; gecko dependencies don't work correctly
        # for Chimera.
          
        TinderUtils::print_log "Clobbering chimera...\n";
        TinderUtils::run_shell_command("pbxbuild -buildstyle \"Deployment\" clean");
          
        my $foo = Cwd::getcwd();
        TinderUtils::print_log "cwd = $foo\n";
          
        my $build_cmd = "pbxbuild -buildstyle ";

        # opt = Deployment, debug = Development.
        if($chimera_build_opt) {
          $build_cmd .=  "\"Deployment\"";  
        } else {
          $build_cmd .=  "\"Development\"";
        }

        # Add   -target NavigatorStatic   for static build.
        if($chimera_build_static) {
          $build_cmd .=  " -target NavigatorStatic";
        }

        $status = TinderUtils::run_shell_command($build_cmd);
        TinderUtils::print_log "Status from pbxbuild: $status\n";
      }

      TinderUtils::print_log "Testing build status...\n";
      if ($status != 0) {
        TinderUtils::print_log "busted, pbxbuild status non-zero\n";
        $post_status = 'busted';
      } elsif (not TinderUtils::BinaryExists("$chimera_dir/build/Navigator.app/Contents/MacOS/$chimera_binary")) {
        TinderUtils::print_log "Error: binary not found: $chimera_dir/build/Navigator.app/Contents/MacOS/$chimera_binary\n";
        $post_status = 'busted';
      } else {
        $post_status = 'success';
      }
    }  
  }

  #
  # Tests...
  #

  # Clean profile out, if set.
  # Assume Chimera always uses ~/Library/Application Support/Chimera/Profiles/default for now.
  if ($chimera_clean_profile) {
    # Warning: workaround chimera bug, delete whole Chimera dir.
    my $chim_profile_dir = "$ENV{HOME}/Library/Application Support/Chimera";
    #my $chim_profile_dir = "$ENV{HOME}/Library/Application Support/Chimera/Profiles/default";

    TinderUtils::print_log "Deleting $chim_profile_dir...\n";
    print "Deleting $chim_profile_dir...\n";
    File::Path::rmtree([$chim_profile_dir], 0, 0);

    if (-e "$chim_profile_dir") {
      TinderUtils::print_log "Error: rmtree('$chim_profile_dir') failed\n";
    }
      
    # Re-create profile.  We have no -CreateProfile, so we instead
    # run the AliveTest first.
  }

  # AliveTest for chimera, about:blank
  if ($chimera_alive_test and $post_status eq 'success') {

    $post_status = TinderUtils::AliveTest("ChimeraAliveTest",
                                          "$chimera_dir/build/Navigator.app/Contents/MacOS",
                                          "Navigator",
                                          "-url \"about:blank\"",  
                                          45);
  }

  # Find the prefs file, remember we have the secret/random salt dir,
  # e.g. /Users/cltbld/Library/Application Support/Chimera/Profiles/default/dyrs1ar8.slt/prefs.js
  # so File::Path::find will find the prefs.js file.
  #
  my $pref_file = "prefs.js";
  my $start_dir = "/Users/cltbld/Library/Application Support/Chimera/Profiles/default";
  my $found = undef;
  my $sub = sub {$pref_file = $File::Find::name, $found++ if $pref_file eq $_};

  File::Find::find($sub, $start_dir);  # Find prefs.js, write this to $pref_file.

  # Set up performance prefs.
  if ($pref_file and ($chimera_layout_performance_test or 1)) {

      # Some tests need browser.dom.window.dump.enabled set to true, so
      # that JS dump() will work in optimized builds.
      if (system("\\grep -s browser.dom.window.dump.enabled $pref_file > /dev/null")) {
	  TinderUtils::print_log "Setting browser.dom.window.dump.enabled\n";
	  open PREFS, ">>$pref_file" or die "can't open $pref_file ($?)\n";
	  print PREFS "user_pref(\"browser.dom.window.dump.enabled\", true);\n";
	  close PREFS;
        } else {
            TinderUtils::print_log "Already set browser.dom.window.dump.enabled\n";
        }

      # Set security prefs to allow us to close our own window,
      # pageloader test (and possibly other tests) needs this on.
      if (system("\\grep -s dom.allow_scripts_to_close_windows $pref_file > /dev/null")) {
          TinderUtils::print_log "Setting dom.allow_scripts_to_close_windows to 2.\n";
          open PREFS, ">>$pref_file" or die "can't open $pref_file ($?)\n";
          print PREFS "user_pref(\"dom.allow_scripts_to_close_windows\", 2);\n";
          close PREFS;
      } else {
          TinderUtils::print_log "Already set dom.allow_scripts_to_close_windows\n";
      }


  }

  # LayoutTest8
  # resource:///res/samples/test8.html
  if ($chimera_test8_test and $post_status eq 'success') {

      if(0) {
          open STDOUT, ">/tmp/foo";
          open STDERR, ">&/tmp/foo";
          select STDOUT; $| = 1;  # make STDOUT unbuffered
          select STDERR; $| = 1;  # make STDERR unbuffered
          print STDERR "hello, world\n";
          chdir("$chimera_dir/build/Navigator.app/Contents/MacOS");
          exec "./Navigator -url \"http://lxr.mozilla.org/seamonkey/source/webshell/tests/viewer/samples/test8.html\"";
          #exec "foo";
      } else {
          $post_status = TinderUtils::AliveTest("ChimeraLayoutTest8Test",
                                                "$chimera_dir/build/Navigator.app/Contents/MacOS",
                                                "Navigator",
                                                "-url \"http://lxr.mozilla.org/seamonkey/source/webshell/tests/viewer/samples/test8.html\"",
                                                20);
      }
  }
  
  # Pageload test.
  if ($chimera_layout_performance_test and $post_status eq 'success') {
    my $layout_time;
    my $test_name = "ChimeraLayoutPerformanceTest";
    my $binary_log = "$test_name.log";

    $layout_time =
      TinderUtils::AliveTestReturnToken($test_name,
                                        "$chimera_dir/build/Navigator.app/Contents/MacOS",
                                        "Navigator",
                                        "-url \"http://64.236.153.13/page-loader/loader.pl?delay=1000&nocache=0&maxcyc=4&timeout=15000&auto=1\"", # spider, 64.236.153.13
                                        800,
                                        "_x_x_mozilla_page_load",
                                        ",");

    if($layout_time) {
        chomp($layout_time);
        my @times = split(',', $layout_time);
        $layout_time = $times[0];  # Set layout time to first number that we scraped.

    } else {
        TinderUtils::print_log "TinderboxPrint:Tp:[CRASH]\n";

        # Run the test a second time.  Getting intermittent crashes, these
        # are expensive to wait, a 2nd run that is successful is still useful.
        # Sorry for the cut & paste. -mcafee

        $layout_time =
          TinderUtils::AliveTestReturnToken($test_name,
                                            "$chimera_dir/build/Navigator.app/Contents/MacOS",
                                            "Navigator",
                                            "-url \"http://64.236.153.13/page-loader/loader.pl?delay=1000&nocache=0&maxcyc=4&timeout=15000&auto=1\"",
                                            800,
                                            "_x_x_mozilla_page_load",
                                            ",");

        # Print failure message if we fail 2nd time.
        unless($layout_time) {
            TinderUtils::print_log "TinderboxPrint:Tp:[CRASH]\n";
        }
    }

    # Set test status.
    if($layout_time) {
        $post_status = 'success';
    } else {
        $post_status = 'testfailed';
    }


    if($post_status eq 'success') {
        my $time = POSIX::strftime "%Y:%m:%d:%H:%M:%S", localtime;

        TinderUtils::print_log "TinderboxPrint:" .
          "<a title=\"Avg of the median per url pageload time\" href=\"http://$Settings::results_server/graph/query.cgi?testname=pageload&tbox=" .
            ::hostname() . "&autoscale=1&days=7&avg=1&showpoint=$time,$layout_time\">Tp:$layout_time" . "ms</a>\n";

        # Pull out detail data from log.
        my $cwd = TinderUtils::get_system_cwd();
        print "cwd = $cwd\n";
        my $raw_data = TinderUtils::extract_token_from_file("$chimera_dir/build/Navigator.app/Contents/MacOS/$binary_log", "_x_x_mozilla_page_load_details", ",");
        chomp($raw_data);

        if($Settings::TestsPhoneHome) {
          TinderUtils::send_results_to_server($layout_time, $raw_data, "pageload", ::hostname());
      }
    }


  }


  # Startup test.
  if ($chimera_startup_test and $post_status eq 'success') {
      my $i;
      my $startuptime;         # Startup time in ms.
      my $agg_startuptime = 0; # Aggregate startup time.
      my $startup_count   = 0; # Number of successful runs.
      my $avg_startuptime = 0; # Average startup time.
      my @times;
      
      for($i=0; $i<10; $i++) {
          # Settle OS.
        TinderUtils::run_system_cmd("sync; sleep 5", 35);
          
          # Generate URL of form file:///<path>/startup-test.html?begin=986869495000
          # Where begin value is current time.
          my ($time, $url, $cwd, $cmd);
          
          #
          # Test for Time::HiRes and report the time.
          $time = Time::PossiblyHiRes::getTime();
          
          $cwd = TinderUtils::get_system_cwd();
          print "cwd = $cwd\n";
          $url  = "\"file:$chimera_dir/../../../startup-test.html?begin=$time\"";
          
          print "url = $url\n";
          
          # Then load startup-test.html, which will pull off the begin argument
          # and compare it to the current time to compute startup time.
          # Since we are iterating here, save off logs as StartupPerformanceTest-0,1,2...
          #
          # -P $Settings::MozProfileName added 3% to startup time, assume one profile
          # and get the 3% back. (http://bugzilla.mozilla.org/show_bug.cgi?id=112767)
          #
          if($post_status eq 'success') {
              $startuptime =
                TinderUtils::AliveTestReturnToken("StartupPerformanceTest-$i",
                                                  "$chimera_dir/build/Navigator.app/Contents/MacOS",
                                                  "Navigator",
                                                  "-url $url",
                                                  $Settings::StartupPerformanceTestTimeout,
                                                  "__startuptime",
                                                  ",");
          }
          
      
          if($startuptime) {
              $post_status = 'success';
              
              # Add our test to the total.
              $startup_count++;
              $agg_startuptime += $startuptime;
              
              # Keep track of the results in an array.
              push(@times, $startuptime);
          } else {
              $post_status = 'testfailed';
          }
      
      } # for loop

      if($post_status eq 'success') {
          TinderUtils::print_log "\nSummary for startup test:\n";
          
          # Print startup times.
          chop(@times);
          my $times_string = join(" ", @times);
          TinderUtils::print_log "times = [$times_string]\n";
          
          # Figure out the average startup time.
          $avg_startuptime = $agg_startuptime / $startup_count;
          TinderUtils::print_log "Average startup time: $avg_startuptime\n";
          
          my $min_startuptime = TinderUtils::min(@times);
          TinderUtils::print_log "Minimum startup time: $min_startuptime\n";
          
          # Old mechanism here, new = TinderboxPrint.
          # print_log "\n\n  __avg_startuptime,$avg_startuptime\n\n";
          # print_log "\n\n  __avg_startuptime,$min_startuptime\n\n";
          
          my $time = POSIX::strftime "%Y:%m:%d:%H:%M:%S", localtime;
          my $print_string = "\n\nTinderboxPrint:<a title=\"Best startup time out of 10 startups\"href=\"http://$Settings::results_server/graph/query.cgi?testname=startup&tbox=" . ::hostname() . "&autoscale=1&days=7&avg=1&showpoint=$time,$min_startuptime\">Ts:" . $min_startuptime . "ms</a>\n\n";
          TinderUtils::print_log "$print_string";
          
          # Report data back to server
          if($Settings::TestsPhoneHome) {
              TinderUtils::print_log "phonehome = 1\n";
              TinderUtils::send_results_to_server($min_startuptime, $times_string,
                                                  "startup", ::hostname());
          }
          
      }
  }
  
  # Pass our status back to calling script.
  return $post_status;
}

# Need to end with a true value, (since we're using "require").
1;
