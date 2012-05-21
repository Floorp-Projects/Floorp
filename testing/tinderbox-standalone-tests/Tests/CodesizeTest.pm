# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Codesize test.  Needs:  cvs checkout mozilla/tools/codesighs
#
# This test can be run in two modes.  One for the whole SeaMonkey
# tree, the other for just the embedding stuff.
#
sub CodesizeTest {
  my ($test_name, $build_dir, $isEmbedTest) = @_;

  my $topsrcdir = "$build_dir/mozilla";

  # test needs this set
  $ENV{MOZ_MAPINFO} = "1";
  $ENV{TINDERBOX_OUTPUT} = "1";
  
  #chdir(".."); # up one level.
  
  my $cwd = get_system_cwd();
  Util::print_log("cwd = $cwd\n");
  
  my $type; # "auto" or "base"
  my $zee;  # Letter that shows up on tbox.
  my $testNameString;
  my $graphName;

  if($isEmbedTest) {
    $testNameString = "Embed"; 
    $type = "base";     # Embed test.
    $zee = "mZ";
    $graphName = "codesize_embed";
  } else {
    if ($Settings::ProductName eq 'Mozilla') {
      $testNameString = "SeaMonkey";
    } else {
      $testNameString = $Settings::ProductName;
    }
    $type = "auto";  # SeaMonkey test.
    $zee = "Z";
    $graphName = "codesize";
  }

  my $new_log   = "Codesize-" . $type . "-new.log";
  my $old_log   = "Codesize-" . $type . "-old.log";
  my $diff_log  = "Codesize-" . $type . "-diff.log";
  my $test_log  = "$test_name.log";
  
  Util::print_log("\$build_dir = $build_dir\n");

  # Clear the logs from the last run, so we can properly test for success.
  unlink("$build_dir/$new_log");
  unlink("$build_dir/$diff_log");
  unlink("$build_dir/$test_log");
  
  my $bash_cmd = "$topsrcdir/tools/codesighs/";
  if ($Settings::CodesizeManifest ne '') {
    $type = '';
  }
  if ($Settings::OS =~ /^WIN/ && $Settings::Compiler ne "gcc") {
    $bash_cmd .= $type . "summary.win.bash";
  } else {
    # Assume Linux for non-windows for now.
    $bash_cmd .= $type . "summary.unix.bash";
  }
 
  my $cmd = ["bash", $bash_cmd];
  push(@{$cmd}, "-o", "$objdir") if ($Settings::ObjDir ne "");
  if ($Settings::CodesizeManifest ne '') {
    push(@{$cmd}, "mozilla/$Settings::CodesizeManifest");
  }
  push(@{$cmd}, $new_log, $old_log, $diff_log);
 
  my $test_result =
    FileBasedTest($test_name, 
                  "$build_dir", 
                  "$build_dir",  # run top of tree, not in dist.
                  $cmd,
                  $Settings::CodesizeTestTimeout,
                  "FAILED", # Fake out failure mode, test file instead.
                  0, 0);    # Timeout means failure.
  
  # Set status based on file creation.
  if (-e "$build_dir/$new_log") {
    Util::print_log("found $build_dir/$new_log\n");
    $test_result = 'success';
    
    # Print diff data to tbox log.
    if (-e "$build_dir/$diff_log") {
      Util::print_logfile("$build_dir/$diff_log", "codesize diff log");
    }
    
    #
    # Extract data.
    #
    my $z_data = Util::extract_token_from_file("$build_dir/$test_log", "__codesize", ":");
    chomp($z_data);
    Util::print_log_test_result_bytes($graphName,
                                "$testNameString: Code + data size of all shared libs & executables",
                                $z_data, $zee, 4);

    if($Settings::TestsPhoneHome) {
      Util::send_results_to_server($z_data, "--", $graphName);
    }
    
    my $zdiff_data = Util::extract_token_from_file("$build_dir/$test_log", "__codesizeDiff", ":");
    chomp($zdiff_data);
    
    # Print out Zdiff if not zero.  Testing "zero" by looking for "+0 ".
    my $zdiff_sample = substr($zdiff_data,0,3);
    if (not ($zdiff_sample eq "+0 ")) {
      Util::print_log("<a title=\"Change from last $zee value (+added/-subtracted)\" TinderboxPrint:" . $zee . "diff:$zdiff_data</a>\n");
    }
    
    # Testing only!  Moves the old log to some unique name for testing.
    # my $time = POSIX::strftime "%d%H%M%S", localtime;
    # rename("$build_dir/$old_log", "$build_dir/$old_log.$time");
    
    # Get ready for next cycle.
    rename("$build_dir/$new_log", "$build_dir/$old_log");
    
        
  } else {
    Util::print_log("Error: $build_dir/$new_log not found.\n");
    $test_result = 'buildfailed';
  }
}

1;
