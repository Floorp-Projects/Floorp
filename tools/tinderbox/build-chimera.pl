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
  my $chimera_startup_test            = 1;
  my $chimera_layout_performance_test = 1;

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
        TinderUtils::run_shell_command("make clean");

        my $foo = Cwd::getcwd();
        TinderUtils::print_log "cwd = $foo\n";
          
        $status = TinderUtils::run_shell_command("make");
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

      $post_status = 
        TinderUtils::LayoutPerformanceTest("ChimeraLayoutPerformanceTest",
                                           "Navigator",
                                           "$chimera_dir/build/Navigator.app/Contents/MacOS",
                                           "-url");

  }


  # Startup test.
  if ($chimera_startup_test and $post_status eq 'success') {

      $post_status =
        TinderUtils::StartupPerformanceTest("ChimeraStartupPerformanceTest",
                                            "Navigator",
                                            "$chimera_dir/build/Navigator.app/Contents/MacOS",
                                            "-url",
                                            "file:$chimera_dir/../../../startup-test.html");      
  }
  
  # Pass our status back to calling script.
  return $post_status;
}

# Need to end with a true value, (since we're using "require").
1;
