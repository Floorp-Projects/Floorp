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
  my $status = TinderUtils::run_shell_command("$Settings::CVS checkout mozilla/camino");

  # hack in the camino prefs, if needed

  return $status;
}


sub ReadHeapDumpToHash($$)
{
  my($dumpfile, $hashref) = @_;

  local(*DUMP_FILE);
  open(DUMP_FILE, "< $dumpfile") || die "Can't open heap output file $dumpfile\n";

  my $section = 0;
  my $zone = "";
  my $default_zone_bytes = 0;
  
  while (<DUMP_FILE>)
  {
    my($line) = $_;
    chomp($line);

    if ($line =~ /^\#/ || $line =~ /^\s*$/ || $line =~ /------------------------/) {    # ignore comments and empty lines
      next;
    }
    
    # use the 'All zones' lines as section markers
    if ($line =~ /^All zones:/) {
      $section ++; 
      next;
    }
    
    if ($section == 2) # we're in the detailed section
    {
      if ($line =~ /^Zone ([^_]+).+\((\d+).+\)/)
      {
        $zone = $1;
        if ($zone eq "DefaultMallocZone") {
          $default_zone_bytes = $2;
        }
        TinderUtils::print_log("  $line\n");
        next;
      }
      
      if ($zone eq "DefaultMallocZone")
      {
        if ($line =~ /(\w+)\s*=\s*(\d+)\s*\((\d+).+\)/)
        {
          my(%entry_hash) = ("count", $2,
                             "bytes", $3);
          $hashref->{$1} = \%entry_hash;
        }
        elsif ($line =~ /^<not.+>\s*=\s*(\d+)\s*\((\d+).+\)/)
        {
          my(%entry_hash) = ("count", $1,
                             "bytes", $2);
          $hashref->{"malloc_block"} = \%entry_hash;
        }
      }
    }
  }
 
  close(DUMP_FILE);
  
  return $default_zone_bytes;
}

sub DumpHash($)
{
  my($hashref) = @_;
  
  print "Dumping values\n";
  
  my($key);
  foreach $key (keys %{$hashref})
  {
    my($value) = $hashref->{$key};
    print "Class $key count $value->{'count'} bytes $value->{'bytes'}\n";
  }
}

sub ClassEntriesEqual($$)
{
  my($hashone, $hashtwo) = @_;
  
  return ($hashone->{"count"} == $hashtwo->{"count"}) and ($hashone->{"bytes"} == $hashtwo->{"bytes"})
}


sub DumpHashDiffs($$)
{
  my($beforehash, $afterhash) = @_;
    
  my($name_width) = 50;
  my($print_format) = "%10s %-${name_width}s %8d %8d\n";

  TinderUtils::print_log(sprintf "%10s %-${name_width}s %8s %8s\n", "", "Class", "Count", "Bytes");
  TinderUtils::print_log("--------------------------------------------------------------------------------\n");
  
  my($key);
  foreach $key (keys %{$beforehash})
  {
    my($beforevalue) = $beforehash->{$key};

    if (exists $afterhash->{$key})
    {
      my($aftervalue) = $afterhash->{$key};
      if (!ClassEntriesEqual($beforevalue, $aftervalue))
      {
        my($count_change) = $aftervalue->{"count"} - $beforevalue->{"count"};
        my($bytes_change) = $aftervalue->{"bytes"} - $beforevalue->{"bytes"};
        
        if ($bytes_change > 0) {
          TinderUtils::print_log(sprintf $print_format, "Leaked", $key, $count_change, $bytes_change);
        } else {
          TinderUtils::print_log(sprintf $print_format, "Freed", $key, $count_change, $bytes_change);
        }
      }
    }
    else
    {
      #$before_only{$key} = $beforevalue;
      TinderUtils::print_log(sprintf $print_format, "Freed", $key, $beforevalue->{'count'}, $beforevalue->{'bytes'});
    }
  }

  # now look for things only in the after cache
  foreach $key (keys %{$afterhash})
  {
    my($value) = $afterhash->{$key};

    if (!exists $beforehash->{$key})
    {
      #$after_only{$key} = $value;
      TinderUtils::print_log(sprintf $print_format, "Leaked", $key, $value->{'count'}, $value->{'bytes'});
    }
  }
 
  TinderUtils::print_log("--------------------------------------------------------------------------------\n");

}


sub ParseHeapOutput($$)
{
  my($beforedump, $afterdump) = @_;
  
  TinderUtils::print_log("Before window open:\n");
  my %before_data;
  my $before_heap = ReadHeapDumpToHash($beforedump, \%before_data);
  
  TinderUtils::print_log("After window open and close:\n");
  my %after_data;
  my $after_heap = ReadHeapDumpToHash($afterdump, \%after_data);

#  DumpHash(\%before_data);
#  DumpHash(\%after_data);

  TinderUtils::print_log("Open/close window leaks:\n");
  DumpHashDiffs(\%before_data, \%after_data);

  my $heap_diff = $after_heap - $before_heap;
  TinderUtils::print_log("TinderboxPrint:<acronym title=\"Per-window leaks\">Lw:".TinderUtils::PrintSize($heap_diff, 3)."B</acronym>\n");
}


sub CaminoWindowLeaksTest($$$$$)
{
  my ($test_name, $build_dir, $binary, $args, $timeout_secs) = @_;

  my($status) = 'success';

  my $beforefile = "/tmp/nav_before_heap.dat";
  my $afterfile  = "/tmp/nav_after_heap.dat";

  my $run_test = 1;   # useful for testing parsing code
  if ($run_test)
  {
    my $close_window_script =<<END_SCRIPT;
tell application "Camino"
  delay 5
  close the first window
end tell
END_SCRIPT
    
    my $new_window_script =<<END_SCRIPT;
tell application "Camino"
  Get URL "about:blank"
  delay 20
  close the first window
end tell
END_SCRIPT
  
    TinderUtils::print_log("Window leak test\n");
  
    my $binary_basename = File::Basename::basename($binary);
    my $binary_dir = File::Basename::dirname($binary);
    my $binary_log = "$build_dir/$test_name.log";
    my $cmd = $binary_basename;
      
    my $pid = TinderUtils::fork_and_log($build_dir, $binary_dir, $cmd, $binary_log);
  
    # close the existing window 
    system("echo '$close_window_script' | osascript");
    # open and close few window to get to a stable point
    system("echo '$new_window_script' | osascript");
  
    # dump before data
    system("heap $pid > $beforefile");
  
    # run the test
    system("echo '$new_window_script' | osascript");
  
    # dump after data
    system("heap $pid > $afterfile");
    
    my $result = TinderUtils::wait_for_pid($pid, $timeout_secs);
  }
  
  ParseHeapOutput($beforefile, $afterfile);

  return $status;
}


sub main {
  my ($mozilla_build_dir) = @_;

  TinderUtils::print_log("Starting camino build.\n");

  # Tell camino to unbuffer stdio, otherwise when we kill the child process
  # stdout & stderr won't get flushed.
  $ENV{MOZ_UNBUFFERED_STDIO} = 1;

  # Pending a config file, stuff things here.
  my $post_status = 'success';  # Success until we report a failure.
  my $status = 0;  # 0 = success

  # Control tests from here, sorry no config file yet.
  my $camino_alive_test              = 1;
  my $camino_test8_test              = 1;
  my $camino_layout_performance_test = 1;
  my $camino_startup_test            = 1;
  my $camino_window_leaks_test       = 0;

  my $safari_layout_performance_test  = 0;

  my $camino_clean_profile = 1;

  # Build flags
  my $camino_build_static = 0;
  my $camino_build_opt    = 1;

  my $camino_dir = "$mozilla_build_dir/mozilla/camino";
  my $embedding_dir = "$mozilla_build_dir/mozilla/embedding/config";
  my $camino_binary = "Camino";

  unless ($Settings::TestOnly) {
    # Checkout/update the camino code.
    $status = checkout($mozilla_build_dir);
    TinderUtils::print_log("Status from checkout: $status\n");
    if ($status != 0) {
      $post_status = 'busted';
    }
    
    # Build camino if we passed the checkout command.
    if ($post_status ne 'busted') {

      #
      # Build embedding/config.
      #
      
      chdir $embedding_dir;
      
      if ($status == 0) {
        $status = TinderUtils::run_shell_command("make");
        TinderUtils::print_log("Status from make: $status\n");
      }
      
      #
      # Build camino.
      #
      
      chdir $camino_dir;
      
      if ($status == 0) {
        TinderUtils::print_log("Deleting binary...\n");
        TinderUtils::DeleteBinary("$camino_dir/build/Camino.app/Contents/MacOS/$camino_binary");
          
        # Always do a clean build; gecko dependencies don't work correctly
        # for Camino.
          
        TinderUtils::print_log("Clobbering camino...\n");
        TinderUtils::run_shell_command("make clean");
          
        $status = TinderUtils::run_shell_command("make");
        TinderUtils::print_log("Status from pbxbuild: $status\n");
      }

      TinderUtils::print_log("Testing build status...\n");
      if ($status != 0) {
        TinderUtils::print_log("busted, pbxbuild status non-zero\n");
        $post_status = 'busted';
      } elsif (not TinderUtils::BinaryExists("$camino_dir/build/Camino.app/Contents/MacOS/$camino_binary")) {
        TinderUtils::print_log("Error: binary not found: $camino_dir/build/Camino.app/Contents/MacOS/$camino_binary\n");
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
  # Assume Camino always uses ~/Library/Application Support/Chimera/Profiles/default for now.
  if ($camino_clean_profile) {
    # Warning: workaround camino bug, delete whole Camino dir.
    my $chim_profile_dir = "$ENV{HOME}/Library/Application Support/Chimera";
    #my $chim_profile_dir = "$ENV{HOME}/Library/Application Support/Chimera/Profiles/default";

    TinderUtils::print_log("Deleting $chim_profile_dir...\n");
    print "Deleting $chim_profile_dir...\n";
    File::Path::rmtree([$chim_profile_dir], 0, 0);

    if (-e "$chim_profile_dir") {
      TinderUtils::print_log("Error: rmtree('$chim_profile_dir') failed\n");
    }
      
    # Re-create profile.  We have no -CreateProfile, so we instead
    # run the AliveTest first.
  }

  # AliveTest for camino, about:blank
  if ($camino_alive_test and $post_status eq 'success') {

    $post_status = TinderUtils::AliveTest("CaminoAliveTest",
                                          "$camino_dir/build/Camino.app/Contents/MacOS",
                                          ["Camino", "-url", "about:blank"],
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
  if ($pref_file and ($camino_layout_performance_test or 1)) {

      # Some tests need browser.dom.window.dump.enabled set to true, so
      # that JS dump() will work in optimized builds.
      if (system("\\grep -s browser.dom.window.dump.enabled $pref_file > /dev/null")) {
	  TinderUtils::print_log("Setting browser.dom.window.dump.enabled\n");
	  open PREFS, ">>$pref_file" or die "can't open $pref_file ($?)\n";
	  print PREFS "user_pref(\"browser.dom.window.dump.enabled\", true);\n";
	  close PREFS;
        } else {
            TinderUtils::print_log("Already set browser.dom.window.dump.enabled\n");
        }

      # Set security prefs to allow us to close our own window,
      # pageloader test (and possibly other tests) needs this on.
      if (system("\\grep -s dom.allow_scripts_to_close_windows $pref_file > /dev/null")) {
          TinderUtils::print_log("Setting dom.allow_scripts_to_close_windows to true.\n");
          open PREFS, ">>$pref_file" or die "can't open $pref_file ($?)\n";
          print PREFS "user_pref(\"dom.allow_scripts_to_close_windows\", true);\n";
          close PREFS;
      } else {
          TinderUtils::print_log("Already set dom.allow_scripts_to_close_windows\n");
      }


  }

  # LayoutTest8
  # resource:///res/samples/test8.html
  if ($camino_test8_test and $post_status eq 'success') {

      if(0) {
          open STDOUT, ">/tmp/foo";
          open STDERR, ">&/tmp/foo";
          select STDOUT; $| = 1;  # make STDOUT unbuffered
          select STDERR; $| = 1;  # make STDERR unbuffered
          print STDERR "hello, world\n";
          chdir("$camino_dir/build/Camino.app/Contents/MacOS");
          exec "./Camino -url \"http://lxr.mozilla.org/seamonkey/source/webshell/tests/viewer/samples/test8.html\"";
          #exec "foo";
      } else {
          $post_status = TinderUtils::AliveTest("CaminoLayoutTest8Test",
                                                "$camino_dir/build/Camino.app/Contents/MacOS",
                                                ["Camino", "-url", "http://lxr.mozilla.org/seamonkey/source/webshell/tests/viewer/samples/test8.html"],
                                                20);
      }
  }
  
  # Pageload test.
  if ($camino_layout_performance_test and $post_status eq 'success') {

      $post_status = 
        TinderUtils::LayoutPerformanceTest("CaminoLayoutPerformanceTest",
                                           "$camino_dir/build/Camino.app/Contents/MacOS",
                                           ["Camino", "-url"]);

  }


  # Pageload test -- Safari.
  if ($safari_layout_performance_test) {
      
      TinderUtils::run_system_cmd("/Applications/Utilities/Safari.app/Contents/MacOS/Safari-wrap.pl http://$Settings::pageload_server/page-loader/loader.pl?delay=1000%26nocache=0%26maxcyc=4%26timeout=$Settings::LayoutPerformanceTestPageTimeout%26auto=1", 30);
        
        # This is a hack.
        TinderUtils::run_system_cmd("sync; sleep 400", 405);

        TinderUtils::run_system_cmd("/Applications/Utilities/Safari.app/Contents/MacOS/Safari-wrap.pl quit", 30);
      
        TinderUtils::print_log "TinderboxPrint:" .
            "<a title=\"Avg of the median per url pageload time\" href=\"http://$Settings::results_server/graph/query.cgi?testname=pageload&tbox=" . ::hostname() . "-aux&autoscale=1&days=7&avg=1\">~Tp" . "</a>\n";
        
  }

  # Startup test.
  if ($camino_startup_test and $post_status eq 'success') {
      $post_status =
        TinderUtils::StartupPerformanceTest("CaminoStartupPerformanceTest",
                                            "Camino",
                                            "$camino_dir/build/Camino.app/Contents/MacOS",
                                            ["-url"],
                                            "file:$camino_dir/../../../startup-test.html");      
  }
  
  if ($camino_window_leaks_test and $post_status eq 'success') {
        $post_status = CaminoWindowLeaksTest("CaminoWindowLeakTest",
                                              "$camino_dir/build/Camino.app/Contents/MacOS/",
                                              "./Camino",
                                              "-url \"http://www.mozilla.org\"",
                                              20);

  }
  
  # Pass our status back to calling script.
  return $post_status;
}

# Need to end with a true value, (since we're using "require").
1;
